#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
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
    virtual AudioRuntimeMetrics metrics() const;
    virtual void resetMetrics();
    virtual void tick();
    const AudioConfig& config() const;

private:
    static void audioTaskFn(void* arg);
    void startTask();
    void stopTask();
    bool lockI2s() const;
    void unlockI2s() const;
    bool ensureDialToneWav();
    bool ensureSpiffsMounted();
    bool generateDialToneWav(const char* path);
    bool openDialToneWav();
    void closeDialToneWav();

    bool driver_installed_ = false;
    bool capture_active_ = false;
    uint8_t capture_clients_mask_ = 0;
    bool playing_ = false;
    bool dial_tone_active_ = false;
    volatile bool running_task_ = false;
    float dial_tone_gain_ = 0.0f;
    float dial_tone_phase_ = 0.0f;
    uint32_t next_dial_tone_push_ms_ = 0;
    uint32_t play_until_ms_ = 0;
    AudioConfig _config;
    FeatureMatrix features_;
    AudioRuntimeMetrics metrics_;
    i2s_config_t _i2s_config{};
    i2s_pin_config_t _i2s_pins{};
    mutable SemaphoreHandle_t i2s_io_mutex_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    static constexpr uint16_t kAudioTaskStackWords = 4096;
    static constexpr uint8_t kAudioTaskPriority = 5;
    bool spiffs_mount_attempted_ = false;
    bool spiffs_ready_ = false;
    bool dial_tone_wav_ready_ = false;
    String dial_tone_wav_path_;
    File dial_tone_file_;
    uint32_t dial_tone_wav_data_offset_ = 44;
    portMUX_TYPE capture_lock_ = portMUX_INITIALIZER_UNLOCKED;
};

#endif  // AUDIO_ENGINE_H
