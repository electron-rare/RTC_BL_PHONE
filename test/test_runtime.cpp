#include <Arduino.h>
#include <unity.h>

#include "audio/AudioEngine.h"
#include "bluetooth/BluetoothManager.h"
#include "core/PlatformProfile.h"
#include "slic/SLICManager.h"
#include "telephone_sfp/TelephoneSFPManager.h"
#include "telephony/TelephonyService.h"

class FakeSlicController : public SlicController {
public:
    bool begin(const SlicPins& pins) override {
        pins_ = pins;
        initialized_ = true;
        return true;
    }

    void setRing(bool enabled) override { ring_enabled_ = enabled; }

    void setLineEnabled(bool enabled) override { line_enabled_ = enabled; }

    bool isHookOff() const override { return hook_off_; }

    void setPowerDown(bool enabled) override { power_down_ = enabled; }

    void tick() override { ++tick_count_; }

    SlicPins pins_ = {};
    bool initialized_ = false;
    bool ring_enabled_ = false;
    bool line_enabled_ = false;
    bool hook_off_ = false;
    bool power_down_ = false;
    uint32_t tick_count_ = 0;
};

class FakeAudioEngine : public AudioEngine {
public:
    bool begin(const AudioConfig&) override {
        begin_called_ = true;
        return true;
    }

    bool playFile(const char* path) override {
        ++play_count_;
        playing_ = true;
        last_path_ = path == nullptr ? "" : path;
        return true;
    }

    bool startCapture() override {
        capture_active_ = true;
        return true;
    }

    size_t readCaptureFrame(int16_t* dst, size_t samples) override {
        for (size_t i = 0; i < samples; ++i) {
            dst[i] = static_cast<int16_t>(i & 0x7F);
        }
        return samples;
    }

    void stopCapture() override { capture_active_ = false; }

    bool supportsFullDuplex() const override { return full_duplex_; }

    bool isPlaying() const override { return playing_; }

    AudioRuntimeMetrics metrics() const override { return runtime_metrics_; }

    void resetMetrics() override { runtime_metrics_ = AudioRuntimeMetrics{}; }

    void tick() override {}

    bool begin_called_ = false;
    bool capture_active_ = false;
    bool playing_ = false;
    bool full_duplex_ = true;
    uint32_t play_count_ = 0;
    String last_path_;
    AudioRuntimeMetrics runtime_metrics_ = {};
};

void setUp() {}
void tearDown() {}

void test_feature_matrix_profiles() {
    const FeatureMatrix a252 = getFeatureMatrix(BoardProfile::ESP32_A252);
    TEST_ASSERT_TRUE(a252.has_bt_classic);
    TEST_ASSERT_TRUE(a252.has_hfp);
    TEST_ASSERT_TRUE(a252.has_full_duplex_i2s);
    TEST_ASSERT_TRUE(a252.has_ble_control);

    const FeatureMatrix s3 = getFeatureMatrix(BoardProfile::ESP32_S3);
    TEST_ASSERT_FALSE(s3.has_bt_classic);
    TEST_ASSERT_FALSE(s3.has_hfp);
    TEST_ASSERT_FALSE(s3.has_full_duplex_i2s);
    TEST_ASSERT_TRUE(s3.has_ble_control);
}

void test_telephony_transitions_ring_to_playback() {
    FakeSlicController slic;
    FakeAudioEngine audio;
    TelephonyService service;

    service.begin(BoardProfile::ESP32_A252, slic, audio);
    service.triggerIncomingRing();
    service.tick();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelephonyState::RINGING),
                          static_cast<int>(service.state()));
    TEST_ASSERT_TRUE(slic.ring_enabled_);

    slic.hook_off_ = true;
    service.tick();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelephonyState::PLAYING_MESSAGE),
                          static_cast<int>(service.state()));
    TEST_ASSERT_FALSE(slic.ring_enabled_);
    TEST_ASSERT_EQUAL_UINT32(1, audio.play_count_);
    TEST_ASSERT_EQUAL_STRING("/welcome.wav", audio.last_path_.c_str());
}

void test_telephony_returns_to_idle_after_hangup() {
    FakeSlicController slic;
    FakeAudioEngine audio;
    TelephonyService service;

    service.begin(BoardProfile::ESP32_A252, slic, audio);
    service.triggerIncomingRing();
    service.tick();
    slic.hook_off_ = true;
    service.tick();

    audio.playing_ = false;
    service.tick();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelephonyState::OFF_HOOK),
                          static_cast<int>(service.state()));

    slic.hook_off_ = false;
    service.tick();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelephonyState::IDLE),
                          static_cast<int>(service.state()));
}

void test_slic_manager_state_updates() {
    FakeSlicController slic;
    SLICManager manager(&slic);

    manager.begin();
    manager.controlCall(true);
    manager.monitorLine();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SLICLineState::RINGING),
                          static_cast<int>(manager.state()));

    slic.hook_off_ = true;
    manager.controlCall(false);
    manager.monitorLine();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SLICLineState::OFF_HOOK),
                          static_cast<int>(manager.state()));
}

void test_audio_metrics_reset() {
    FakeAudioEngine audio;
    audio.runtime_metrics_.frames_requested = 100;
    audio.runtime_metrics_.drop_frames = 3;
    audio.runtime_metrics_.underrun_count = 1;
    audio.resetMetrics();

    const AudioRuntimeMetrics metrics = audio.metrics();
    TEST_ASSERT_EQUAL_UINT32(0, metrics.frames_requested);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.drop_frames);
    TEST_ASSERT_EQUAL_UINT32(0, metrics.underrun_count);
}

void test_bluetooth_feature_gating() {
    BluetoothManager bt;
    TEST_ASSERT_TRUE(bt.begin(BoardProfile::ESP32_S3));
    TEST_ASSERT_FALSE(bt.startHFP());
    TEST_ASSERT_TRUE(bt.startBLE());

    TEST_ASSERT_TRUE(bt.begin(BoardProfile::ESP32_A252));
    TEST_ASSERT_TRUE(bt.startHFP());
}

void test_telephone_sfp_bridge() {
    FakeSlicController slic;
    FakeAudioEngine audio;
    TelephonyService service;
    TelephoneSFPManager manager;

    service.begin(BoardProfile::ESP32_A252, slic, audio);
    manager.attachService(&service);
    manager.triggerIncomingCall();
    manager.monitorState();

    TEST_ASSERT_EQUAL_INT(static_cast<int>(TelephonyState::RINGING),
                          static_cast<int>(manager.state()));
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_feature_matrix_profiles);
    RUN_TEST(test_telephony_transitions_ring_to_playback);
    RUN_TEST(test_telephony_returns_to_idle_after_hangup);
    RUN_TEST(test_slic_manager_state_updates);
    RUN_TEST(test_audio_metrics_reset);
    RUN_TEST(test_bluetooth_feature_gating);
    RUN_TEST(test_telephone_sfp_bridge);
    UNITY_END();
}

void loop() {}
