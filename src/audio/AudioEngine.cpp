#include "audio/AudioEngine.h"

#include <SPIFFS.h>

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
constexpr size_t kStereoChannels = 2;
constexpr float kDialToneAttackMs = 25.0f;
constexpr float kDialToneReleaseMs = 40.0f;
constexpr uint32_t kDialToneWavSeconds = 1;
constexpr char kDialToneWavPrefix[] = "/dialtone_425_";
constexpr bool kDialToneUseWav = false;
constexpr TickType_t kI2sWriteTimeoutTicks = pdMS_TO_TICKS(2);
}

namespace {
int16_t clampInt16(float value) {
    if (value > static_cast<float>(std::numeric_limits<int16_t>::max())) {
        return std::numeric_limits<int16_t>::max();
    }
    if (value < static_cast<float>(std::numeric_limits<int16_t>::min())) {
        return std::numeric_limits<int16_t>::min();
    }
    return static_cast<int16_t>(value);
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
      play_until_ms_(0),
      features_(getFeatureMatrix(detectBoardProfile())) {}

AudioEngine::~AudioEngine() {
    end();
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

bool AudioEngine::begin(const AudioConfig& config) {
    end();
    _config = config;

    const i2s_mode_t mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER | I2S_MODE_TX |
        ((_config.enable_capture && features_.has_full_duplex_i2s) ? I2S_MODE_RX : 0));

    _i2s_config = {
        .mode = mode,
        .sample_rate = _config.sample_rate,
        .bits_per_sample = _config.bits_per_sample,
        .channel_format = _config.channel_format,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = _config.dma_buf_count,
        .dma_buf_len = _config.dma_buf_len,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };

    _i2s_pins = {
        .bck_io_num = _config.bck_pin,
        .ws_io_num = _config.ws_pin,
        .data_out_num = _config.data_out_pin,
        .data_in_num = _config.data_in_pin,
    };

    if (i2s_driver_install(_config.port, &_i2s_config, 0, nullptr) != ESP_OK) {
        Serial.println("[AudioEngine] i2s_driver_install failed");
        driver_installed_ = false;
        return false;
    }
    if (i2s_set_pin(_config.port, &_i2s_pins) != ESP_OK) {
        Serial.println("[AudioEngine] i2s_set_pin failed");
        i2s_driver_uninstall(_config.port);
        driver_installed_ = false;
        return false;
    }

    if (i2s_io_mutex_ == nullptr) {
        i2s_io_mutex_ = xSemaphoreCreateMutex();
        if (i2s_io_mutex_ == nullptr) {
            Serial.println("[AudioEngine] i2s mutex unavailable");
        }
    }

    i2s_zero_dma_buffer(_config.port);
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
    closeDialToneWav();
    dial_tone_wav_ready_ = false;
    dial_tone_wav_path_ = String(kDialToneWavPrefix) + String(_config.sample_rate) + ".wav";
    if (kDialToneUseWav) {
        ensureDialToneWav();
    }
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
    portENTER_CRITICAL(&capture_lock_);
    capture_clients_mask_ = 0U;
    capture_active_ = false;
    portEXIT_CRITICAL(&capture_lock_);
    closeDialToneWav();
    if (i2s_io_mutex_ != nullptr) {
        vSemaphoreDelete(i2s_io_mutex_);
        i2s_io_mutex_ = nullptr;
    }
    i2s_driver_uninstall(_config.port);
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
    // Firmware skeleton: this marks a playback window for telephony flow.
    playing_ = true;
    play_until_ms_ = millis() + 2500;
    Serial.printf("[AudioEngine] play request: %s\n", path);
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
    size_t bytes_read = 0;
    if (i2s_read(_config.port, dst, byte_count, &bytes_read, 2) != ESP_OK || bytes_read == 0) {
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

    metrics_.frames_requested += static_cast<uint32_t>(samples);
    const size_t byte_count = samples * sizeof(int16_t);
    size_t bytes_read = 0;
    if (i2s_read(_config.port, dst, byte_count, &bytes_read, 0) != ESP_OK || bytes_read == 0) {
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
    size_t bytes_written = 0;
    if (i2s_write(_config.port, src, byte_count, &bytes_written, kI2sWriteTimeoutTicks) != ESP_OK) {
        unlockI2s();
        return 0;
    }
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
    if (kDialToneUseWav) {
        ensureDialToneWav();
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

AudioRuntimeMetrics AudioEngine::metrics() const {
    return metrics_;
}

void AudioEngine::resetMetrics() {
    metrics_ = AudioRuntimeMetrics{};
}

void AudioEngine::tick() {
    if (playing_ && millis() >= play_until_ms_) {
        playing_ = false;
    }

    if (!driver_installed_ || playing_) {
        return;
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

    int16_t frame[kDialToneChunkFrames * kStereoChannels] = {0};
    const float phase_step = (kTwoPi * kDialToneHz) / static_cast<float>(_config.sample_rate);
    const size_t chunk_samples = kDialToneChunkFrames * kStereoChannels;
    const size_t chunk_bytes = chunk_samples * sizeof(int16_t);
    uint8_t pcm_raw[kDialToneChunkFrames * kStereoChannels * sizeof(int16_t)] = {0};
    bool wav_ready = false;
    size_t wav_filled = 0;

    if (kDialToneUseWav && ensureDialToneWav() && openDialToneWav()) {
        while (wav_filled < chunk_bytes) {
            const int got = dial_tone_file_.read(pcm_raw + wav_filled, chunk_bytes - wav_filled);
            if (got > 0) {
                wav_filled += static_cast<size_t>(got);
                continue;
            }
            if (!dial_tone_file_.seek(dial_tone_wav_data_offset_)) {
                break;
            }
        }
        wav_ready = (wav_filled == chunk_bytes);
    }

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

        int16_t sample_l = 0;
        int16_t sample_r = 0;
        if (wav_ready) {
            const size_t l_idx = i * kStereoChannels;
            const size_t r_idx = l_idx + 1;
            const size_t l_byte = l_idx * sizeof(int16_t);
            const size_t r_byte = r_idx * sizeof(int16_t);
            sample_l = static_cast<int16_t>(
                static_cast<uint16_t>(pcm_raw[l_byte]) |
                static_cast<uint16_t>(static_cast<uint16_t>(pcm_raw[l_byte + 1]) << 8));
            sample_r = static_cast<int16_t>(
                static_cast<uint16_t>(pcm_raw[r_byte]) |
                static_cast<uint16_t>(static_cast<uint16_t>(pcm_raw[r_byte + 1]) << 8));
        } else {
            const int16_t sample = static_cast<int16_t>(std::sin(dial_tone_phase_) * static_cast<float>(kDialToneAmplitude));
            sample_l = sample;
            sample_r = sample;
            dial_tone_phase_ += phase_step;
            if (dial_tone_phase_ >= kTwoPi) {
                dial_tone_phase_ -= kTwoPi;
            }
        }

        const float gain = dial_tone_gain_ * kDialToneLinearGain;
        frame[(i * kStereoChannels)] = clampInt16(static_cast<float>(sample_l) * gain);
        frame[(i * kStereoChannels) + 1] = clampInt16(static_cast<float>(sample_r) * gain);
    }

    const size_t requested_samples = kDialToneChunkFrames * kStereoChannels;
    const size_t written_samples = writePlaybackFrame(frame, requested_samples);
    if (written_samples == 0U) {
        // Retry fast when TX queue/mutex was temporarily unavailable.
        next_dial_tone_push_ms_ = now + 1U;
        return;
    }

    const size_t written_frames = written_samples / kStereoChannels;
    const uint32_t chunk_ms = static_cast<uint32_t>((1000U * written_frames) / _config.sample_rate);
    next_dial_tone_push_ms_ = now + (chunk_ms == 0U ? 1U : chunk_ms);
}

const AudioConfig& AudioEngine::config() const {
    return _config;
}

bool AudioEngine::ensureSpiffsMounted() {
    if (spiffs_mount_attempted_) {
        return spiffs_ready_;
    }
    spiffs_mount_attempted_ = true;
    spiffs_ready_ = SPIFFS.begin(false);
    if (!spiffs_ready_) {
        Serial.println("[AudioEngine] SPIFFS not available (dial tone WAV fallback)");
    }
    return spiffs_ready_;
}

bool AudioEngine::generateDialToneWav(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return false;
    }
    if (!ensureSpiffsMounted()) {
        return false;
    }

    SPIFFS.remove(path);
    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.printf("[AudioEngine] cannot create %s\n", path);
        return false;
    }

    const uint16_t channels = static_cast<uint16_t>(kStereoChannels);
    const uint16_t bits_per_sample = 16;
    const uint32_t bytes_per_sample = bits_per_sample / 8U;
    const uint32_t frames = _config.sample_rate * kDialToneWavSeconds;
    const uint32_t data_bytes = frames * channels * bytes_per_sample;
    const uint32_t riff_size = 36U + data_bytes;
    const uint32_t byte_rate = _config.sample_rate * channels * bytes_per_sample;
    const uint16_t block_align = static_cast<uint16_t>(channels * bytes_per_sample);

    auto write_u16 = [&](uint16_t v) {
        uint8_t b[2] = {static_cast<uint8_t>(v & 0xFFU), static_cast<uint8_t>((v >> 8) & 0xFFU)};
        return file.write(b, sizeof(b)) == sizeof(b);
    };
    auto write_u32 = [&](uint32_t v) {
        uint8_t b[4] = {static_cast<uint8_t>(v & 0xFFU), static_cast<uint8_t>((v >> 8) & 0xFFU),
                        static_cast<uint8_t>((v >> 16) & 0xFFU), static_cast<uint8_t>((v >> 24) & 0xFFU)};
        return file.write(b, sizeof(b)) == sizeof(b);
    };

    bool ok = true;
    ok &= file.write(reinterpret_cast<const uint8_t*>("RIFF"), 4) == 4;
    ok &= write_u32(riff_size);
    ok &= file.write(reinterpret_cast<const uint8_t*>("WAVE"), 4) == 4;
    ok &= file.write(reinterpret_cast<const uint8_t*>("fmt "), 4) == 4;
    ok &= write_u32(16U);
    ok &= write_u16(1U);
    ok &= write_u16(channels);
    ok &= write_u32(_config.sample_rate);
    ok &= write_u32(byte_rate);
    ok &= write_u16(block_align);
    ok &= write_u16(bits_per_sample);
    ok &= file.write(reinterpret_cast<const uint8_t*>("data"), 4) == 4;
    ok &= write_u32(data_bytes);
    if (!ok) {
        file.close();
        SPIFFS.remove(path);
        return false;
    }

    int16_t pcm[kDialToneChunkFrames * kStereoChannels] = {0};
    const float phase_step = (kTwoPi * kDialToneHz) / static_cast<float>(_config.sample_rate);
    float phase = 0.0f;
    uint32_t written_frames = 0;
    while (written_frames < frames) {
        const uint32_t remaining = frames - written_frames;
        const uint32_t this_chunk = std::min<uint32_t>(static_cast<uint32_t>(kDialToneChunkFrames), remaining);
        for (uint32_t i = 0; i < this_chunk; ++i) {
            const int16_t s = static_cast<int16_t>(std::sin(phase) * static_cast<float>(kDialToneAmplitude));
            pcm[(i * kStereoChannels)] = s;
            pcm[(i * kStereoChannels) + 1] = s;
            phase += phase_step;
            if (phase >= kTwoPi) {
                phase -= kTwoPi;
            }
        }
        const size_t bytes = static_cast<size_t>(this_chunk * kStereoChannels * sizeof(int16_t));
        if (file.write(reinterpret_cast<const uint8_t*>(pcm), bytes) != bytes) {
            file.close();
            SPIFFS.remove(path);
            return false;
        }
        written_frames += this_chunk;
    }

    file.close();
    return true;
}

bool AudioEngine::ensureDialToneWav() {
    if (!dial_tone_wav_path_.isEmpty() && dial_tone_wav_ready_) {
        return true;
    }
    if (dial_tone_wav_path_.isEmpty()) {
        dial_tone_wav_path_ = String(kDialToneWavPrefix) + String(_config.sample_rate) + ".wav";
    }
    if (!ensureSpiffsMounted()) {
        return false;
    }
    if (!SPIFFS.exists(dial_tone_wav_path_.c_str())) {
        if (!generateDialToneWav(dial_tone_wav_path_.c_str())) {
            return false;
        }
    }
    dial_tone_wav_ready_ = SPIFFS.exists(dial_tone_wav_path_.c_str());
    return dial_tone_wav_ready_;
}

bool AudioEngine::openDialToneWav() {
    if (!dial_tone_wav_ready_) {
        return false;
    }
    if (dial_tone_file_) {
        return true;
    }
    dial_tone_file_ = SPIFFS.open(dial_tone_wav_path_.c_str(), FILE_READ);
    if (!dial_tone_file_) {
        dial_tone_wav_ready_ = false;
        return false;
    }
    if (!dial_tone_file_.seek(dial_tone_wav_data_offset_)) {
        dial_tone_file_.close();
        dial_tone_wav_ready_ = false;
        return false;
    }
    return true;
}

void AudioEngine::closeDialToneWav() {
    if (dial_tone_file_) {
        dial_tone_file_.close();
    }
}
