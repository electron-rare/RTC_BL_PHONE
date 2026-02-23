#include "audio/AudioEngine.h"

#include <SD_MMC.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {
constexpr float kDialToneHz = 425.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr int16_t kDialToneAmplitude = 32000;
constexpr float kDialToneLinearGain = 1.14f;
constexpr size_t kDialToneChunkFrames = 160;
constexpr size_t kMaxChannels = 2;
constexpr float kDialToneAttackMs = 25.0f;
constexpr float kDialToneReleaseMs = 40.0f;
constexpr TickType_t kI2sWriteTimeoutMs = 2;
constexpr TickType_t kI2sReadTimeoutMs = 2;
constexpr size_t kPlaybackCopyBytes = 1024;

int16_t clampInt16(float value) {
    if (value > static_cast<float>(std::numeric_limits<int16_t>::max())) {
        return std::numeric_limits<int16_t>::max();
    }
    if (value < static_cast<float>(std::numeric_limits<int16_t>::min())) {
        return std::numeric_limits<int16_t>::min();
    }
    return static_cast<int16_t>(value);
}

int bitsPerSampleToInt(i2s_bits_per_sample_t bits) {
    switch (bits) {
        case I2S_BITS_PER_SAMPLE_24BIT:
            return 24;
        case I2S_BITS_PER_SAMPLE_32BIT:
            return 32;
        case I2S_BITS_PER_SAMPLE_16BIT:
        default:
            return 16;
    }
}
}  // namespace

AudioConfig defaultAudioConfigForProfile(BoardProfile profile) {
    AudioConfig cfg;
    if (profile == BoardProfile::ESP32_S3) {
        cfg.sample_rate = 16000;
        cfg.bck_pin = 17;
        cfg.ws_pin = 18;
        cfg.data_out_pin = 21;
        cfg.data_in_pin = 39;
        cfg.enable_capture = true;
    } else {
        // AI Thinker A252 defaults (ESP32-A1S + ES8388).
        cfg.sample_rate = 16000;
        cfg.bck_pin = 27;
        cfg.ws_pin = 25;
        cfg.data_out_pin = 26;
        cfg.data_in_pin = 35;
        cfg.enable_capture = true;
    }
    return cfg;
}

AudioEngine::AudioEngine()
    : driver_installed_(false),
      capture_active_(false),
      capture_clients_mask_(0),
      playing_(false),
      features_(getFeatureMatrix(detectBoardProfile())) {
    wav_stream_.setOutput(i2s_stream_);
    wav_stream_.setDecoder(&wav_decoder_);
    wav_copy_.setCheckAvailable(false);
    wav_copy_.setCheckAvailableForWrite(false);
    wav_copy_.setMinCopySize(sizeof(int16_t));
    wav_copy_.setRetry(2);
    wav_copy_.setRetryDelay(2);
}

AudioEngine::~AudioEngine() {
    end();
}

size_t AudioEngine::activeChannelCount(i2s_channel_fmt_t channel_format) {
    switch (channel_format) {
        case I2S_CHANNEL_FMT_ONLY_LEFT:
        case I2S_CHANNEL_FMT_ONLY_RIGHT:
            return 1;
        default:
            return 2;
    }
}

bool AudioEngine::lockI2s() const {
    if (i2s_io_mutex_ == nullptr) {
        return true;
    }
    return xSemaphoreTake(i2s_io_mutex_, pdMS_TO_TICKS(1)) == pdTRUE;
}

void AudioEngine::unlockI2s() const {
    if (i2s_io_mutex_ != nullptr) {
        xSemaphoreGive(i2s_io_mutex_);
    }
}

bool AudioEngine::ensureSdMounted() {
    if (sd_mount_attempted_) {
        return sd_ready_;
    }

    sd_mount_attempted_ = true;
    sd_ready_ = SD_MMC.begin();
    if (!sd_ready_) {
        Serial.println("[AudioEngine] SD_MMC mount failed");
    }
    return sd_ready_;
}

void AudioEngine::stopPlaybackFile() {
    wav_copy_.end();
    wav_stream_.end();
    if (playback_file_) {
        playback_file_.close();
    }
    playback_path_ = "";
    playback_data_remaining_ = 0;
    playback_input_channels_ = 0;
    playing_ = false;
}

bool AudioEngine::prepareWavPlayback(File& file, const char* path) {
    (void)path;
    if (!file) {
        return false;
    }
    // WAV parsing/format conversion is delegated to audio-tools WAVDecoder.
    return true;
}

bool AudioEngine::begin(const AudioConfig& config) {
    end();
    _config = config;

    const bool full_duplex = (_config.enable_capture && features_.has_full_duplex_i2s);
    const audio_tools::RxTxMode mode = full_duplex ? audio_tools::RXTX_MODE : audio_tools::TX_MODE;
    auto i2s_cfg = i2s_stream_.defaultConfig(mode);
    i2s_cfg.port_no = static_cast<int>(_config.port);
    i2s_cfg.sample_rate = _config.sample_rate;
    i2s_cfg.bits_per_sample = bitsPerSampleToInt(_config.bits_per_sample);
    i2s_cfg.channels = static_cast<int>(activeChannelCount(_config.channel_format));
    i2s_cfg.channel_format = _config.channel_format;
    i2s_cfg.pin_bck = _config.bck_pin;
    i2s_cfg.pin_ws = _config.ws_pin;
    i2s_cfg.pin_data = _config.data_out_pin;
    i2s_cfg.pin_data_rx = _config.data_in_pin;
    i2s_cfg.buffer_count = _config.dma_buf_count;
    i2s_cfg.buffer_size = _config.dma_buf_len;
    i2s_cfg.auto_clear = true;

    if (!i2s_stream_.begin(i2s_cfg)) {
        Serial.println("[AudioEngine] i2s begin failed");
        driver_installed_ = false;
        return false;
    }

    if (i2s_stream_.driver() != nullptr) {
        i2s_stream_.driver()->setWaitTimeReadMs(kI2sReadTimeoutMs);
        i2s_stream_.driver()->setWaitTimeWriteMs(kI2sWriteTimeoutMs);
    }

    if (i2s_io_mutex_ == nullptr) {
        i2s_io_mutex_ = xSemaphoreCreateMutex();
        if (i2s_io_mutex_ == nullptr) {
            Serial.println("[AudioEngine] i2s mutex unavailable");
        }
    }

    driver_installed_ = true;
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = 0U;
    capture_active_ = false;
    portEXIT_CRITICAL(&capture_lock_);
    capture_active_ = false;
    playing_ = false;
    dial_tone_active_ = false;
    dial_tone_gain_ = 0.0f;
    dial_tone_phase_ = 0.0f;
    next_dial_tone_push_ms_ = 0;
    stopPlaybackFile();
    startTask();
    Serial.printf("[AudioEngine] ready (full_duplex=%s)\n",
                  supportsFullDuplex() ? "true" : "false");
    return true;
}

void AudioEngine::end() {
    if (!driver_installed_) {
        return;
    }
    stopTask();
    stopDialTone();
    stopPlaybackFile();
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = 0U;
    capture_active_ = false;
    portEXIT_CRITICAL(&capture_lock_);
    if (i2s_io_mutex_ != nullptr) {
        vSemaphoreDelete(i2s_io_mutex_);
        i2s_io_mutex_ = nullptr;
    }
    i2s_stream_.end();
    driver_installed_ = false;
}

void AudioEngine::audioTaskFn(void* arg) {
    auto* self = static_cast<AudioEngine*>(arg);
    while (self != nullptr && self->running_task_) {
        self->tick();
        const bool audio_busy = self->capture_active_ || self->dial_tone_active_ ||
                                (self->dial_tone_gain_ > 0.0005f) || self->playing_;
        vTaskDelay(pdMS_TO_TICKS(audio_busy ? 1U : 6U));
    }
    if (self != nullptr) {
        self->task_handle_ = nullptr;
    }
    vTaskDelete(nullptr);
}

void AudioEngine::startTask() {
    if (!driver_installed_ || task_handle_ != nullptr) {
        return;
    }
    running_task_ = true;
    const BaseType_t rc = xTaskCreatePinnedToCore(
        audioTaskFn,
        "audio_engine",
        kAudioTaskStackWords,
        this,
        kAudioTaskPriority,
        &task_handle_,
        tskNO_AFFINITY);
    if (rc != pdPASS) {
        running_task_ = false;
        task_handle_ = nullptr;
        Serial.println("[AudioEngine] failed to start audio task");
    }
}

void AudioEngine::stopTask() {
    if (task_handle_ == nullptr) {
        return;
    }
    running_task_ = false;
    vTaskDelay(pdMS_TO_TICKS(10));
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

bool AudioEngine::playFile(const char* path) {
    if (!driver_installed_ || path == nullptr || path[0] == '\0') {
        return false;
    }
    if (!ensureSdMounted()) {
        return false;
    }

    stopDialTone();
    stopPlaybackFile();

    playback_file_ = SD_MMC.open(path, FILE_READ);
    if (!playback_file_) {
        Serial.printf("[AudioEngine] sd file not found: %s\n", path);
        return false;
    }

    if (!prepareWavPlayback(playback_file_, path)) {
        stopPlaybackFile();
        return false;
    }

    if (!wav_stream_.begin()) {
        stopPlaybackFile();
        Serial.printf("[AudioEngine] wav decoder begin failed: %s\n", path);
        return false;
    }
    wav_copy_.begin(wav_stream_, playback_file_);
    playback_path_ = path;
    playing_ = true;
    Serial.printf("[AudioEngine] play sd wav (audio-tools): %s\n", path);
    return true;
}

bool AudioEngine::requestCapture(CaptureClient client) {
    if (!driver_installed_) {
        return false;
    }
    const uint8_t bit = static_cast<uint8_t>(client);
    if (bit == 0U) {
        return false;
    }
    if (!supportsFullDuplex() && playing_) {
        return false;
    }

    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = static_cast<uint8_t>(capture_clients_mask_ | bit);
    capture_active_ = (capture_clients_mask_ != 0U);
    portEXIT_CRITICAL(&capture_lock_);
    return true;
}

void AudioEngine::releaseCapture(CaptureClient client) {
    const uint8_t bit = static_cast<uint8_t>(client);
    if (bit == 0U) {
        return;
    }
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = static_cast<uint8_t>(capture_clients_mask_ & static_cast<uint8_t>(~bit));
    capture_active_ = (capture_clients_mask_ != 0U);
    portEXIT_CRITICAL(&capture_lock_);
}

bool AudioEngine::startCapture() {
    return requestCapture(CAPTURE_CLIENT_GENERIC);
}

size_t AudioEngine::readCaptureFrame(int16_t* dst, size_t samples) {
    if (!capture_active_ || !driver_installed_ || dst == nullptr || samples == 0) {
        return 0;
    }
    if (!lockI2s()) {
        return 0;
    }

    metrics_.frames_requested += static_cast<uint32_t>(samples);
    const uint32_t start_ms = millis();
    const size_t byte_count = samples * sizeof(int16_t);
    size_t bytes_read = i2s_stream_.readBytes(reinterpret_cast<uint8_t*>(dst), byte_count);
    if (bytes_read == 0) {
        std::memset(dst, 0, byte_count);
        metrics_.underrun_count++;
        metrics_.drop_frames += static_cast<uint32_t>(samples);
        metrics_.last_latency_ms = millis() - start_ms;
        metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
        unlockI2s();
        return 0;
    }
    const size_t read_samples = bytes_read / sizeof(int16_t);
    metrics_.frames_read += static_cast<uint32_t>(read_samples);
    if (read_samples < samples) {
        metrics_.drop_frames += static_cast<uint32_t>(samples - read_samples);
    }
    metrics_.last_latency_ms = millis() - start_ms;
    metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
    unlockI2s();
    return read_samples;
}

size_t AudioEngine::readCaptureFrameNonBlocking(int16_t* dst, size_t samples) {
    if (!capture_active_ || !driver_installed_ || dst == nullptr || samples == 0) {
        return 0;
    }
    if (!lockI2s()) {
        return 0;
    }

    if (i2s_stream_.driver() != nullptr) {
        i2s_stream_.driver()->setWaitTimeReadMs(0);
    }

    metrics_.frames_requested += static_cast<uint32_t>(samples);
    const size_t byte_count = samples * sizeof(int16_t);
    const size_t bytes_read = i2s_stream_.readBytes(reinterpret_cast<uint8_t*>(dst), byte_count);

    if (i2s_stream_.driver() != nullptr) {
        i2s_stream_.driver()->setWaitTimeReadMs(kI2sReadTimeoutMs);
    }

    if (bytes_read == 0) {
        unlockI2s();
        return 0;
    }

    const size_t read_samples = bytes_read / sizeof(int16_t);
    metrics_.frames_read += static_cast<uint32_t>(read_samples);
    if (read_samples < samples) {
        metrics_.drop_frames += static_cast<uint32_t>(samples - read_samples);
    }
    unlockI2s();
    return read_samples;
}

size_t AudioEngine::writePlaybackFrame(const int16_t* src, size_t samples) {
    if (!driver_installed_ || src == nullptr || samples == 0) {
        return 0;
    }
    if (!lockI2s()) {
        return 0;
    }

    const size_t byte_count = samples * sizeof(int16_t);
    const size_t bytes_written = i2s_stream_.write(reinterpret_cast<const uint8_t*>(src), byte_count);
    unlockI2s();
    return bytes_written / sizeof(int16_t);
}

void AudioEngine::stopCapture() {
    releaseCapture(CAPTURE_CLIENT_GENERIC);
}

bool AudioEngine::startDialTone() {
    if (!driver_installed_) {
        return false;
    }
    const bool was_active = dial_tone_active_;
    dial_tone_active_ = true;
    if (!was_active && dial_tone_gain_ <= 0.0001f) {
        dial_tone_phase_ = 0.0f;
    }
    next_dial_tone_push_ms_ = 0;
    return true;
}

void AudioEngine::stopDialTone() {
    dial_tone_active_ = false;
    dial_tone_gain_ = 0.0f;
    next_dial_tone_push_ms_ = 0;
}

bool AudioEngine::isDialToneActive() const {
    return dial_tone_active_ || dial_tone_gain_ > 0.001f;
}

bool AudioEngine::supportsFullDuplex() const {
    return features_.has_full_duplex_i2s;
}

bool AudioEngine::isPlaying() const {
    return playing_;
}

bool AudioEngine::isSdReady() const {
    return sd_ready_;
}

AudioRuntimeMetrics AudioEngine::metrics() const {
    return metrics_;
}

void AudioEngine::resetMetrics() {
    metrics_ = AudioRuntimeMetrics{};
}

bool AudioEngine::streamPlaybackChunk() {
    if (!playback_file_) {
        stopPlaybackFile();
        return false;
    }

    const size_t copied = wav_copy_.copyBytes(kPlaybackCopyBytes);
    if (copied > 0U) {
        return true;
    }

    if (!playback_file_.available()) {
        stopPlaybackFile();
        return false;
    }

    return true;
}

void AudioEngine::tick() {
    if (!driver_installed_) {
        return;
    }

    if (playing_) {
        if (streamPlaybackChunk()) {
            return;
        }
    }

    const bool tone_requested = dial_tone_active_;
    const bool tone_tail_active = dial_tone_gain_ > 0.0005f;
    if (!tone_requested && !tone_tail_active) {
        return;
    }

    const uint32_t now = millis();
    if (next_dial_tone_push_ms_ != 0 && now < next_dial_tone_push_ms_) {
        return;
    }

    const size_t channels = activeChannelCount(_config.channel_format);
    if (channels == 0U || channels > kMaxChannels) {
        return;
    }

    int16_t frame[kDialToneChunkFrames * kMaxChannels] = {0};
    const float phase_step = (kTwoPi * kDialToneHz) / static_cast<float>(_config.sample_rate);

    const float attack_step =
        1.0f / std::max(1.0f, (static_cast<float>(_config.sample_rate) * (kDialToneAttackMs / 1000.0f)));
    const float release_step =
        1.0f / std::max(1.0f, (static_cast<float>(_config.sample_rate) * (kDialToneReleaseMs / 1000.0f)));
    for (size_t i = 0; i < kDialToneChunkFrames; ++i) {
        if (dial_tone_active_) {
            dial_tone_gain_ = std::min(1.0f, dial_tone_gain_ + attack_step);
        } else {
            dial_tone_gain_ = std::max(0.0f, dial_tone_gain_ - release_step);
        }

        const int16_t sample = static_cast<int16_t>(std::sin(dial_tone_phase_) * static_cast<float>(kDialToneAmplitude));
        dial_tone_phase_ += phase_step;
        if (dial_tone_phase_ >= kTwoPi) {
            dial_tone_phase_ -= kTwoPi;
        }

        const int16_t out = clampInt16(static_cast<float>(sample) * dial_tone_gain_ * kDialToneLinearGain);
        for (size_t ch = 0; ch < channels; ++ch) {
            frame[i * channels + ch] = out;
        }
    }

    const size_t requested_samples = kDialToneChunkFrames * channels;
    const size_t written_samples = writePlaybackFrame(frame, requested_samples);
    if (written_samples == 0U) {
        next_dial_tone_push_ms_ = now + 1U;
        return;
    }

    const size_t written_frames = written_samples / channels;
    const uint32_t chunk_ms = static_cast<uint32_t>((1000U * written_frames) / _config.sample_rate);
    next_dial_tone_push_ms_ = now + (chunk_ms == 0U ? 1U : chunk_ms);
}

const AudioConfig& AudioEngine::config() const {
    return _config;
}
