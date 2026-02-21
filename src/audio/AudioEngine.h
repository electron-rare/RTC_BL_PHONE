#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
#include <driver/i2s.h>

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
    bool enable_capture = true;
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
};

AudioConfig defaultAudioConfigForProfile(BoardProfile profile);

class AudioEngine {
public:
    virtual ~AudioEngine();
    AudioEngine();
    virtual bool begin(const AudioConfig& config);
    virtual void end();
    virtual bool playFile(const char* path);
    virtual bool startCapture();
    virtual size_t readCaptureFrame(int16_t* dst, size_t samples);
    virtual void stopCapture();
    virtual bool supportsFullDuplex() const;
    virtual bool isPlaying() const;
    virtual AudioRuntimeMetrics metrics() const;
    virtual void resetMetrics();
    virtual void tick();
    const AudioConfig& config() const;

private:
    bool driver_installed_ = false;
    bool capture_active_ = false;
    bool playing_ = false;
    uint32_t play_until_ms_ = 0;
    AudioConfig _config;
    FeatureMatrix features_;
    AudioRuntimeMetrics metrics_;
    i2s_config_t _i2s_config{};
    i2s_pin_config_t _i2s_pins{};
};

#endif  // AUDIO_ENGINE_H
