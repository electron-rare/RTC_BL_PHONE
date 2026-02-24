#include "audio/AudioEngine.h"

#include <SD_MMC.h>
#include <esp_dsp.h>

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
constexpr size_t kAdcDspFirTaps = 5U;
constexpr float kDspDcBlockR = 0.995f;
constexpr float kDspHighPassHz = 250.0f;
constexpr float kDspLowPassHz = 3400.0f;
constexpr float kDspAdcScale = 1.0f / 2048.0f;
constexpr float kDspPostGain = 1.0f;
constexpr uint8_t kAdcDspMinFftDownsample = 1U;
constexpr uint8_t kAdcDspMaxFftDownsample = 64U;
constexpr float kDialToneAttackMs = 25.0f;
constexpr float kDialToneReleaseMs = 40.0f;
constexpr TickType_t kI2sWriteTimeoutMs = 2;
constexpr TickType_t kI2sReadTimeoutMs = 2;
constexpr size_t kPlaybackCopyBytes = 1024;
constexpr int16_t kAdcRawMax = 4095;
constexpr int16_t kAdcMidScale = kAdcRawMax / 2;

int16_t clampInt16(float value) {
    if (value > static_cast<float>(std::numeric_limits<int16_t>::max())) {
        return std::numeric_limits<int16_t>::max();
    }
    if (value < static_cast<float>(std::numeric_limits<int16_t>::min())) {
        return std::numeric_limits<int16_t>::min();
    }
    return static_cast<int16_t>(value);
}

void biquadLowPassCoeff(float sample_rate_hz, float frequency_hz, float q, float& b0, float& b1, float& b2, float& a1, float& a2) {
    if (frequency_hz <= 0.0f || sample_rate_hz <= 0.0f || q <= 0.0f) {
        b0 = 1.0f;
        b1 = 0.0f;
        b2 = 0.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        return;
    }
    const float omega = kTwoPi * frequency_hz / sample_rate_hz;
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * q);
    const float b0o = (1.0f - cs) / 2.0f;
    const float b1o = 1.0f - cs;
    const float b2o = (1.0f - cs) / 2.0f;
    const float a0 = 1.0f + alpha;
    const float a1o = -2.0f * cs;
    const float a2o = 1.0f - alpha;

    b0 = b0o / a0;
    b1 = b1o / a0;
    b2 = b2o / a0;
    a1 = a1o / a0;
    a2 = a2o / a0;
}

void biquadHighPassCoeff(float sample_rate_hz, float frequency_hz, float q, float& b0, float& b1, float& b2, float& a1, float& a2) {
    if (frequency_hz <= 0.0f || sample_rate_hz <= 0.0f || q <= 0.0f) {
        b0 = 1.0f;
        b1 = 0.0f;
        b2 = 0.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        return;
    }
    const float omega = kTwoPi * frequency_hz / sample_rate_hz;
    const float sn = std::sin(omega);
    const float cs = std::cos(omega);
    const float alpha = sn / (2.0f * q);
    const float b0o = (1.0f + cs) / 2.0f;
    const float b1o = -(1.0f + cs);
    const float b2o = (1.0f + cs) / 2.0f;
    const float a0 = 1.0f + alpha;
    const float a1o = -2.0f * cs;
    const float a2o = 1.0f - alpha;

    b0 = b0o / a0;
    b1 = b1o / a0;
    b2 = b2o / a0;
    a1 = a1o / a0;
    a2 = a2o / a0;
}

float processBiquad(float input, float& b0, float& b1, float& b2, float& a1, float& a2, float& z1, float& z2) {
    const float y = b0 * input + z1;
    z1 = b1 * input - a1 * y + z2;
    z2 = b2 * input - a2 * y;
    return y;
}

float clampFloat(float value, float lo, float hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
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

void AudioEngine::initAdcDspChain(uint32_t sample_rate_hz) {
    const float sr = static_cast<float>(sample_rate_hz == 0U ? kAdcDspDefaultSampleRateHz : sample_rate_hz);

    const float high_cut = std::min(sr * 0.45f - 20.0f, kDspLowPassHz);
    const float low_cut = std::min(std::max(kDspHighPassHz, 10.0f), sr * 0.45f - 100.0f);

    biquadHighPassCoeff(sr, low_cut, 0.707f, adc_dsp_biquad_hp_b0_, adc_dsp_biquad_hp_b1_, adc_dsp_biquad_hp_b2_,
                       adc_dsp_biquad_hp_a1_, adc_dsp_biquad_hp_a2_);
    biquadLowPassCoeff(sr, high_cut > 0.0f ? high_cut : 1.0f, 0.707f, adc_dsp_biquad_lp_b0_, adc_dsp_biquad_lp_b1_,
                      adc_dsp_biquad_lp_b2_, adc_dsp_biquad_lp_a1_, adc_dsp_biquad_lp_a2_);
    resetAdcDspState();
    initAdcFftDspBackend();
    adc_dsp_chain_enabled_ = true;
    Serial.printf("[AudioEngine] ADC DSP chain enabled (sr=%u, hp=%.1fHz, lp=%.1fHz)\n",
                  static_cast<unsigned>(sample_rate_hz),
                  low_cut,
                  high_cut > 0.0f ? high_cut : 1.0f);
}

void AudioEngine::initAdcFftDspBackend() {
    adc_dsp_fft_probe_backend_ready_ = false;
    if (!adc_dsp_fft_probe_enabled_ || !adc_fft_enabled_ || kAdcDspFftWindowSamples == 0U) {
        return;
    }

    const esp_err_t ret = dsps_fft2r_init_fc32(nullptr, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("[AudioEngine] FFT backend init failed: %d\n", ret);
        return;
    }
    adc_dsp_fft_probe_backend_ready_ = true;
}

void AudioEngine::deinitAdcFftDspBackend() {
    if (!adc_dsp_fft_probe_backend_ready_) {
        return;
    }

    dsps_fft2r_deinit_fc32();
    adc_dsp_fft_probe_backend_ready_ = false;
}

void AudioEngine::updateAdcDspConfig(const AudioConfig& cfg) {
    adc_dsp_chain_enabled_ = (use_adc_capture_ && cfg.adc_dsp_enabled);
    adc_fft_enabled_ = (adc_dsp_chain_enabled_ && cfg.adc_fft_enabled);
    adc_dsp_fft_probe_enabled_ = adc_fft_enabled_;
    adc_dsp_fft_downsample_ = cfg.adc_dsp_fft_downsample;
    if (adc_dsp_fft_downsample_ < kAdcDspMinFftDownsample) {
        adc_dsp_fft_downsample_ = kAdcDspMinFftDownsample;
    } else if (adc_dsp_fft_downsample_ > kAdcDspMaxFftDownsample) {
        adc_dsp_fft_downsample_ = kAdcDspMaxFftDownsample;
    }

    const uint16_t max_ignore_bin = static_cast<uint16_t>((kAdcDspFftWindowSamples / 2U > 0U) ? (kAdcDspFftWindowSamples / 2U - 1U) : 0U);
    adc_fft_ignore_low_bin_ = std::min<uint16_t>(cfg.adc_fft_ignore_low_bin, max_ignore_bin);
    adc_fft_ignore_high_bin_ = std::min<uint16_t>(cfg.adc_fft_ignore_high_bin, max_ignore_bin);

    if (!adc_dsp_chain_enabled_) {
        deinitAdcFftDspBackend();
        return;
    }

    initAdcDspChain(cfg.sample_rate);
}

void AudioEngine::resetAdcDspState() {
    std::memset(adc_dsp_fir_state_, 0, sizeof(adc_dsp_fir_state_));
    adc_dsp_fir_pos_ = 0U;
    adc_dsp_prev_input_ = 0.0f;
    adc_dsp_prev_output_ = 0.0f;
    adc_dsp_biquad_hp_z1_ = 0.0f;
    adc_dsp_biquad_hp_z2_ = 0.0f;
    adc_dsp_biquad_lp_z1_ = 0.0f;
    adc_dsp_biquad_lp_z2_ = 0.0f;
    std::memset(adc_dsp_fft_buffer_, 0, sizeof(adc_dsp_fft_buffer_));
    adc_dsp_fft_head_ = 0U;
    adc_dsp_fft_fill_ = 0U;
    adc_dsp_fft_decimator_ = 0U;
    metrics_.adc_fft_peak_bin = 0U;
    const uint32_t effective_downsample = std::max<uint32_t>(1U, static_cast<uint32_t>(adc_dsp_fft_downsample_));
    metrics_.adc_fft_probe_rate_hz =
        static_cast<uint16_t>(std::max(1U, _config.sample_rate / std::max(1U, effective_downsample)));
    metrics_.adc_fft_peak_freq_hz = 0.0f;
    metrics_.adc_fft_peak_magnitude = 0.0f;
}

float AudioEngine::applyDcBlocker(float sample) {
    const float filtered = sample - adc_dsp_prev_input_ + (kDspDcBlockR * adc_dsp_prev_output_);
    adc_dsp_prev_input_ = sample;
    adc_dsp_prev_output_ = filtered;
    return filtered;
}

float AudioEngine::applyFirNoiseReduction(float sample) {
    adc_dsp_fir_state_[adc_dsp_fir_pos_] = sample;

    // FIR taps: 1/16 * [1, 4, 6, 4, 1] (soft anti-alias + anti-click).
    constexpr float kFirCoeff[kAdcDspFirTaps] = {0.0625f, 0.25f, 0.375f, 0.25f, 0.0625f};

    float result = 0.0f;
    uint8_t idx = adc_dsp_fir_pos_;
    for (size_t tap = 0U; tap < kAdcDspFirTaps; ++tap) {
        const size_t fir_idx = (idx + kAdcDspFirTaps - tap) % kAdcDspFirTaps;
        result += kFirCoeff[tap] * adc_dsp_fir_state_[fir_idx];
    }

    adc_dsp_fir_pos_ = static_cast<uint8_t>((adc_dsp_fir_pos_ + 1U) % kAdcDspFirTaps);
    return result;
}

int16_t AudioEngine::applyBiquadChain(float sample) {
    const float hp = processBiquad(sample, adc_dsp_biquad_hp_b0_, adc_dsp_biquad_hp_b1_, adc_dsp_biquad_hp_b2_,
                                  adc_dsp_biquad_hp_a1_, adc_dsp_biquad_hp_a2_, adc_dsp_biquad_hp_z1_,
                                  adc_dsp_biquad_hp_z2_);
    const float lp = processBiquad(hp, adc_dsp_biquad_lp_b0_, adc_dsp_biquad_lp_b1_, adc_dsp_biquad_lp_b2_,
                                 adc_dsp_biquad_lp_a1_, adc_dsp_biquad_lp_a2_, adc_dsp_biquad_lp_z1_,
                                 adc_dsp_biquad_lp_z2_);
    return clampInt16(clampFloat(lp * kDspPostGain * 32768.0f, -32768.0f, 32767.0f));
}

void AudioEngine::appendAdcFftSample(float sample) {
    if (!adc_dsp_fft_probe_enabled_ || adc_dsp_fft_downsample_ == 0U || kAdcDspFftWindowSamples == 0U) {
        return;
    }

    if (++adc_dsp_fft_decimator_ < adc_dsp_fft_downsample_) {
        return;
    }
    adc_dsp_fft_decimator_ = 0U;

    adc_dsp_fft_buffer_[adc_dsp_fft_head_] = sample;
    adc_dsp_fft_head_ = static_cast<uint8_t>((adc_dsp_fft_head_ + 1U) % kAdcDspFftWindowSamples);
    if (adc_dsp_fft_fill_ < kAdcDspFftWindowSamples) {
        ++adc_dsp_fft_fill_;
        return;
    }

    runAdcFftProbe();
}

void AudioEngine::runAdcFftProbe() {
    if (!adc_dsp_fft_probe_enabled_ || adc_dsp_fft_fill_ < kAdcDspFftWindowSamples || kAdcDspFftWindowSamples == 0U) {
        return;
    }

    const size_t kHalfBins = kAdcDspFftWindowSamples / 2U;
    if (kHalfBins < 2U) {
        return;
    }
    float best_power = 0.0f;
    uint16_t best_bin = 0U;
    const uint32_t effective_downsample = std::max<uint32_t>(1U, static_cast<uint32_t>(adc_dsp_fft_downsample_));
    const float probe_sr = static_cast<float>(std::max(1U, _config.sample_rate)) / static_cast<float>(effective_downsample);
    const uint16_t ignore_low = std::min<uint16_t>(adc_fft_ignore_low_bin_, static_cast<uint16_t>(kHalfBins - 1U));
    const uint16_t ignore_high = std::min<uint16_t>(adc_fft_ignore_high_bin_, static_cast<uint16_t>(kHalfBins));
    const uint16_t upper_limit = (ignore_high >= kHalfBins) ? 1U : static_cast<uint16_t>(kHalfBins - ignore_high);

    for (size_t i = 0U; i < kAdcDspFftWindowSamples; ++i) {
        const size_t src_idx = (adc_dsp_fft_head_ + i) % kAdcDspFftWindowSamples;
        const float phase = static_cast<float>(i) / static_cast<float>(kAdcDspFftWindowSamples - 1U);
        const float sample = adc_dsp_fft_buffer_[src_idx] * (0.5f - 0.5f * std::cos(kTwoPi * phase));
        adc_dsp_fft_complex_buffer_[i * 2U] = sample;
        adc_dsp_fft_complex_buffer_[i * 2U + 1U] = 0.0f;
    }

    if (adc_dsp_fft_probe_backend_ready_) {
        esp_err_t ret = dsps_fft2r_fc32(adc_dsp_fft_complex_buffer_, static_cast<int>(kAdcDspFftWindowSamples));
        if (ret != ESP_OK) {
            Serial.printf("[AudioEngine] dsps_fft2r_fc32 failed: %d\n", ret);
            return;
        }
        ret = dsps_bit_rev_fc32(adc_dsp_fft_complex_buffer_, static_cast<int>(kAdcDspFftWindowSamples));
        if (ret != ESP_OK) {
            Serial.printf("[AudioEngine] dsps_bit_rev_fc32 failed: %d\n", ret);
            return;
        }
        ret = dsps_cplx2reC_fc32(adc_dsp_fft_complex_buffer_, static_cast<int>(kAdcDspFftWindowSamples));
        if (ret != ESP_OK) {
            Serial.printf("[AudioEngine] dsps_cplx2reC_fc32 failed: %d\n", ret);
            return;
        }
    } else {
        return;
    }

    for (size_t bin = 1U; bin < kHalfBins; ++bin) {
        if (bin <= ignore_low || bin >= upper_limit) {
            continue;
        }
        const float re = adc_dsp_fft_complex_buffer_[bin * 2U];
        const float im = adc_dsp_fft_complex_buffer_[bin * 2U + 1U];
        const float power = (re * re) + (im * im);
        if (power > best_power) {
            best_power = power;
            best_bin = static_cast<uint16_t>(bin);
        }
    }

    metrics_.adc_fft_peak_bin = best_bin;
    metrics_.adc_fft_peak_magnitude = std::sqrt(best_power);
    metrics_.adc_fft_peak_freq_hz = best_bin == 0U ? 0.0f
                                                  : (static_cast<float>(best_bin) *
                                                     (probe_sr / static_cast<float>(kAdcDspFftWindowSamples)));
}

int16_t AudioEngine::processAdcSample(int16_t raw_sample) {
    float sample = static_cast<float>(raw_sample) * kDspAdcScale;
    if (!adc_dsp_chain_enabled_) {
        return clampInt16(sample * kDspPostGain * 32768.0f);
    }

    sample = applyDcBlocker(sample);
    sample = applyFirNoiseReduction(sample);
    appendAdcFftSample(sample);
    return applyBiquadChain(sample);
}

bool AudioEngine::begin(const AudioConfig& config) {
    end();
    _config = config;
    adc_capture_pin_ = config.capture_adc_pin;
    use_adc_capture_ = (adc_capture_pin_ >= 0);

    if (use_adc_capture_) {
        if (adc_capture_pin_ < 0 || adc_capture_pin_ > 39) {
            Serial.printf("[AudioEngine] invalid ADC pin for capture: %d\n", adc_capture_pin_);
            return false;
        }

        pinMode(adc_capture_pin_, INPUT);
        analogReadResolution(12);
        analogSetPinAttenuation(adc_capture_pin_, ADC_11db);
        adc_capture_sample_interval_us_ = std::max(1U, 1000000U / _config.sample_rate);
    } else {
        adc_capture_sample_interval_us_ = 0U;
    }
    updateAdcDspConfig(config);
    next_adc_capture_us_ = 0U;

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
    deinitAdcFftDspBackend();
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
    const bool was_active = capture_active_;
    capture_clients_mask_ = static_cast<uint8_t>(capture_clients_mask_ | bit);
    capture_active_ = (capture_clients_mask_ != 0U);
    if (capture_active_ && !was_active && use_adc_capture_ && adc_dsp_chain_enabled_) {
        resetAdcDspState();
        next_adc_capture_us_ = 0U;
    }
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
    if (use_adc_capture_) {
        return captureFromAdc(dst, samples, true);
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
    if (use_adc_capture_) {
        return captureFromAdc(dst, samples, false);
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

size_t AudioEngine::captureFromAdc(int16_t* dst, size_t samples, bool blocking) {
    if (dst == nullptr || samples == 0) {
        return 0;
    }

    const uint32_t start_ms = millis();
    metrics_.frames_requested += static_cast<uint32_t>(samples);
    size_t captured = 0;

    if (next_adc_capture_us_ == 0U) {
        next_adc_capture_us_ = static_cast<uint64_t>(micros());
    }

    if (adc_capture_sample_interval_us_ == 0U) {
        adc_capture_sample_interval_us_ = 1000000U / std::max(1U, _config.sample_rate);
    }

    while (captured < samples) {
        const uint64_t target_us = next_adc_capture_us_;
        const uint64_t now_us = micros();
        if (!blocking && now_us < target_us) {
            break;
        }
        if (blocking && now_us < target_us) {
            delayMicroseconds(static_cast<unsigned long>(target_us - now_us));
        }

        const int raw = analogRead(adc_capture_pin_);
        const int16_t centered = static_cast<int16_t>(raw - kAdcMidScale);
        dst[captured] = processAdcSample(centered);
        ++captured;
        next_adc_capture_us_ = target_us + adc_capture_sample_interval_us_;
    }

    metrics_.frames_read += static_cast<uint32_t>(captured);
    if (captured < samples) {
        metrics_.underrun_count++;
        metrics_.drop_frames += static_cast<uint32_t>(samples - captured);
    }

    metrics_.last_latency_ms = millis() - start_ms;
    metrics_.max_latency_ms = std::max(metrics_.max_latency_ms, metrics_.last_latency_ms);
    return captured;
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
