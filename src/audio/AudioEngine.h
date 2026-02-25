#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
#include <AudioTools.h>
#include <FS.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "core/PlatformProfile.h"

struct AudioConfig {
    i2s_port_t port = I2S_NUM_0;
    uint32_t sample_rate = 16000;
    i2s_bits_per_sample_t bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_channel_fmt_t channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    int bck_pin = 27;
    int ws_pin = 25;
    int data_out_pin = 26;
    int data_in_pin = 35;
    int capture_adc_pin = -1;
    bool enable_capture = true;
    bool adc_dsp_enabled = true;
    bool adc_fft_enabled = true;
    uint8_t adc_dsp_fft_downsample = 2U;
    uint16_t adc_fft_ignore_low_bin = 1U;
    uint16_t adc_fft_ignore_high_bin = 1U;
    uint8_t dma_buf_count = 8;
    uint16_t dma_buf_len = 256;
};

struct AudioRuntimeMetrics {
    uint32_t frames_requested = 0;
    uint32_t frames_read = 0;
    uint32_t drop_frames = 0;
    uint32_t underrun_count = 0;
    uint32_t last_latency_ms = 0;
    uint32_t max_latency_ms = 0;
    uint16_t adc_fft_peak_bin = 0;
    uint16_t adc_fft_probe_rate_hz = 0;
    float adc_fft_peak_freq_hz = 0.0f;
    float adc_fft_peak_magnitude = 0.0f;
};

AudioConfig defaultAudioConfigForProfile(BoardProfile profile);

class AudioEngine {
public:
    enum CaptureClient : uint8_t {
        CAPTURE_CLIENT_GENERIC = 0x01,
        CAPTURE_CLIENT_TELEPHONY = 0x02,
        CAPTURE_CLIENT_BLUETOOTH = 0x04,
    };

    virtual ~AudioEngine();
    AudioEngine();
    virtual bool begin(const AudioConfig& config);
    virtual void end();
    virtual bool playFile(const char* path);
    virtual bool requestCapture(CaptureClient client);
    virtual void releaseCapture(CaptureClient client);
    virtual bool startCapture();
    virtual size_t readCaptureFrame(int16_t* dst, size_t samples);
    virtual size_t readCaptureFrameNonBlocking(int16_t* dst, size_t samples);
    virtual size_t writePlaybackFrame(const int16_t* src, size_t samples);
    virtual void stopCapture();
    virtual bool startDialTone();
    virtual void stopDialTone();
    virtual bool isDialToneActive() const;
    virtual bool supportsFullDuplex() const;
    virtual bool isPlaying() const;
    virtual bool isSdReady() const;
    virtual bool isReady() const;
    virtual AudioRuntimeMetrics metrics() const;
    virtual void resetMetrics();
    virtual void tick();
    const AudioConfig& config() const;

private:
    static size_t activeChannelCount(i2s_channel_fmt_t channel_format);
    static void audioTaskFn(void* arg);
    size_t captureFromAdc(int16_t* dst, size_t samples, bool blocking);
    void initAdcDspChain(uint32_t sample_rate_hz);
    int16_t processAdcSample(int16_t raw_sample);
    void resetAdcDspState();
    float applyDcBlocker(float sample);
    float applyFirNoiseReduction(float sample);
    int16_t applyBiquadChain(float sample);
    void appendAdcFftSample(float sample);
    void runAdcFftProbe();
    void initAdcFftDspBackend();
    void deinitAdcFftDspBackend();
    void startTask();
    void stopTask();
    bool lockI2s() const;
    void unlockI2s() const;
    bool ensureAudioStorageMounted();
    void stopPlaybackFile();
    bool prepareWavPlayback(File& file, const char* path);
    bool streamPlaybackChunk();
    void updateAdcDspConfig(const AudioConfig& cfg);

    bool driver_installed_ = false;
    bool capture_active_ = false;
    uint8_t capture_clients_mask_ = 0;
    bool playing_ = false;
    bool dial_tone_active_ = false;
    volatile bool running_task_ = false;
    float dial_tone_gain_ = 0.0f;
    float dial_tone_phase_ = 0.0f;
    uint32_t next_dial_tone_push_ms_ = 0;
    bool audio_fs_mount_attempted_ = false;
    bool audio_fs_ready_ = false;
    bool audio_fs_is_fat_ = false;
    fs::FS* audio_fs_ = nullptr;
    File playback_file_;
    String playback_path_;
    uint32_t playback_data_remaining_ = 0;
    uint16_t playback_input_channels_ = 0;
    AudioConfig _config;
    FeatureMatrix features_;
    AudioRuntimeMetrics metrics_;
    int adc_capture_pin_ = -1;
    uint32_t adc_capture_sample_interval_us_ = 0;
    uint64_t next_adc_capture_us_ = 0;
    bool use_adc_capture_ = false;
    bool adc_dsp_chain_enabled_ = false;
    bool adc_fft_enabled_ = false;
    uint8_t adc_dsp_fft_downsample_ = kAdcDspDefaultFftDownsample;
    uint16_t adc_fft_ignore_low_bin_ = 1U;
    uint16_t adc_fft_ignore_high_bin_ = 1U;
    static constexpr uint32_t kAdcDspDefaultSampleRateHz = 16000U;
    static constexpr uint8_t kAdcDspDefaultFftDownsample = 2U;
    float adc_dsp_prev_input_ = 0.0f;
    float adc_dsp_prev_output_ = 0.0f;
    float adc_dsp_fir_state_[5U] = {0.0f};
    uint8_t adc_dsp_fir_pos_ = 0U;
    float adc_dsp_biquad_hp_b0_ = 1.0f;
    float adc_dsp_biquad_hp_b1_ = 0.0f;
    float adc_dsp_biquad_hp_b2_ = 0.0f;
    float adc_dsp_biquad_hp_a1_ = 0.0f;
    float adc_dsp_biquad_hp_a2_ = 0.0f;
    float adc_dsp_biquad_hp_z1_ = 0.0f;
    float adc_dsp_biquad_hp_z2_ = 0.0f;
    float adc_dsp_biquad_lp_b0_ = 1.0f;
    float adc_dsp_biquad_lp_b1_ = 0.0f;
    float adc_dsp_biquad_lp_b2_ = 0.0f;
    float adc_dsp_biquad_lp_a1_ = 0.0f;
    float adc_dsp_biquad_lp_a2_ = 0.0f;
    float adc_dsp_biquad_lp_z1_ = 0.0f;
    float adc_dsp_biquad_lp_z2_ = 0.0f;
    static constexpr size_t kAdcDspFftWindowSamples = 64U;
    float adc_dsp_fft_buffer_[kAdcDspFftWindowSamples] = {0.0f};
    uint8_t adc_dsp_fft_head_ = 0U;
    uint8_t adc_dsp_fft_fill_ = 0U;
    uint8_t adc_dsp_fft_decimator_ = 0U;
    float adc_dsp_fft_complex_buffer_[kAdcDspFftWindowSamples * 2U] = {0.0f};
    bool adc_dsp_fft_probe_enabled_ = false;
    bool adc_dsp_fft_probe_backend_ready_ = false;
    audio_tools::I2SStream i2s_stream_;
    audio_tools::WAVDecoder wav_decoder_;
    audio_tools::EncodedAudioStream wav_stream_;
    audio_tools::StreamCopy wav_copy_;
    mutable SemaphoreHandle_t i2s_io_mutex_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    static constexpr uint16_t kAudioTaskStackWords = 4096;
    static constexpr uint8_t kAudioTaskPriority = 5;
    portMUX_TYPE capture_lock_ = portMUX_INITIALIZER_UNLOCKED;
};

#endif  // AUDIO_ENGINE_H
