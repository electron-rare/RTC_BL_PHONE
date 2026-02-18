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
    virtual ~AudioEngine() = default;
    AudioEngine();
    virtual bool begin(const AudioConfig& config);
    virtual bool playFile(const char* path);
    virtual bool startCapture();
    virtual size_t readCaptureFrame(int16_t* dst, size_t samples);
    virtual void stopCapture();
    virtual bool supportsFullDuplex() const;
    virtual bool isPlaying() const;
    virtual AudioRuntimeMetrics metrics() const;
    virtual void resetMetrics();
    virtual void tick();

private:
    bool driver_installed_;
    bool capture_active_;
    bool playing_;
    uint32_t play_until_ms_;
    AudioConfig config_;
    FeatureMatrix features_;
    AudioRuntimeMetrics metrics_;
};

#endif  // AUDIO_ENGINE_H
