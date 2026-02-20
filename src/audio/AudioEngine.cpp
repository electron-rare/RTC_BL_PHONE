#include "audio/AudioEngine.h"

#include <algorithm>
#include <cstring>

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
      playing_(false),
      play_until_ms_(0),
      features_(getFeatureMatrix(detectBoardProfile())) {}

AudioEngine::~AudioEngine() {
    end();
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

    i2s_zero_dma_buffer(_config.port);
    driver_installed_ = true;
    capture_active_ = false;
    playing_ = false;
    Serial.printf("[AudioEngine] ready (full_duplex=%s)\n",
                  supportsFullDuplex() ? "true" : "false");
    return true;
}

void AudioEngine::end() {
    if (!driver_installed_) {
        return;
    }
    stopCapture();
    i2s_driver_uninstall(_config.port);
    driver_installed_ = false;
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

bool AudioEngine::startCapture() {
    if (!driver_installed_) {
        return false;
    }
    if (!supportsFullDuplex() && playing_) {
        return false;
    }
    capture_active_ = true;
    return true;
}

size_t AudioEngine::readCaptureFrame(int16_t* dst, size_t samples) {
    if (!capture_active_ || !driver_installed_ || dst == nullptr || samples == 0) {
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
        return 0;
    }
    const size_t read_samples = bytes_read / sizeof(int16_t);
    metrics_.frames_read += static_cast<uint32_t>(read_samples);
    if (read_samples < samples) {
        metrics_.drop_frames += static_cast<uint32_t>(samples - read_samples);
    }
    metrics_.last_latency_ms = millis() - start_ms;
    metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
    return read_samples;
}

void AudioEngine::stopCapture() {
    capture_active_ = false;
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
}

const AudioConfig& AudioEngine::config() const {
    return _config;
}
