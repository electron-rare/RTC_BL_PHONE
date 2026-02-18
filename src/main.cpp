#include <Arduino.h>
#include <ArduinoJson.h>

#include "audio/AudioEngine.h"
#include "core/PlatformProfile.h"
#include "slic/Ks0835SlicController.h"
#include "telephony/TelephonyService.h"
#include "web/WebServerManager.h"

#ifndef UNIT_TEST
namespace {
constexpr uint32_t kSerialBaud = 115200;

BoardProfile g_profile = BoardProfile::ESP32_A252;
FeatureMatrix g_features = getFeatureMatrix(BoardProfile::ESP32_A252);

Ks0835SlicController g_slic;
AudioEngine g_audio;
TelephonyService g_telephony;
WebServerManager g_web(80);

String g_serial_line;

SlicPins slicPinsForProfile(BoardProfile profile) {
    if (profile == BoardProfile::ESP32_S3) {
        return SlicPins{
            .pin_rm = 5,
            .pin_fr = 6,
            .pin_shk = 4,
            .pin_line_enable = 7,
            .pin_pd = 8,
            .hook_active_high = false,
        };
    }

    return SlicPins{
        .pin_rm = 26,
        .pin_fr = 33,
        .pin_shk = 27,
        .pin_line_enable = 25,
        .pin_pd = 14,
        .hook_active_high = false,
    };
}

void appendAudioMetrics(JsonObject root) {
    const AudioRuntimeMetrics metrics = g_audio.metrics();
    root["audio_frames_requested"] = metrics.frames_requested;
    root["audio_frames_read"] = metrics.frames_read;
    root["audio_drop_frames"] = metrics.drop_frames;
    root["audio_underrun_count"] = metrics.underrun_count;
    root["audio_last_latency_ms"] = metrics.last_latency_ms;
    root["audio_max_latency_ms"] = metrics.max_latency_ms;
}

void printStatusLine() {
    DynamicJsonDocument doc(512);
    doc["board_profile"] = boardProfileToString(g_profile);
    doc["telephony"] = telephonyStateToString(g_telephony.state());
    doc["hook"] = g_slic.isHookOff() ? "OFF_HOOK" : "ON_HOOK";
    doc["full_duplex"] = g_audio.supportsFullDuplex();
    appendAudioMetrics(doc.to<JsonObject>());
    String payload;
    serializeJson(doc, payload);
    Serial.println(payload);
}

bool onWebControl(const String& action, const JsonVariantConst& payload) {
    if (action == "call") {
        g_telephony.triggerIncomingRing();
        return true;
    }
    if (action == "capture_start") {
        return g_audio.startCapture();
    }
    if (action == "capture_stop") {
        g_audio.stopCapture();
        return true;
    }
    if (action == "play_message") {
        const char* path = payload["path"] | "/welcome.wav";
        return g_audio.playFile(path);
    }
    if (action == "reset_metrics") {
        g_audio.resetMetrics();
        return true;
    }
    return false;
}

void onWebStatus(JsonObject obj) {
    obj["board_profile"] = boardProfileToString(g_profile);
    obj["telephony"] = telephonyStateToString(g_telephony.state());
    obj["hook"] = g_slic.isHookOff() ? "OFF_HOOK" : "ON_HOOK";
    obj["full_duplex"] = g_audio.supportsFullDuplex();
    appendAudioMetrics(obj);
}

void printHelp() {
    Serial.println("[RTC_BL_PHONE] Commands:");
    Serial.println("  PING");
    Serial.println("  STATUS");
    Serial.println("  CALL");
    Serial.println("  CAPTURE_START");
    Serial.println("  CAPTURE_STOP");
    Serial.println("  PLAY [/path.wav]");
    Serial.println("  RESET_METRICS");
}

void handleSerialCommand(String line) {
    line.trim();
    if (line.isEmpty()) {
        return;
    }

    if (line == "PING") {
        Serial.println("PONG");
        return;
    }
    if (line == "STATUS") {
        printStatusLine();
        return;
    }
    if (line == "CALL") {
        g_telephony.triggerIncomingRing();
        Serial.println("OK CALL");
        return;
    }
    if (line == "CAPTURE_START") {
        Serial.println(g_audio.startCapture() ? "OK CAPTURE_START" : "ERR CAPTURE_START");
        return;
    }
    if (line == "CAPTURE_STOP") {
        g_audio.stopCapture();
        Serial.println("OK CAPTURE_STOP");
        return;
    }
    if (line.startsWith("PLAY")) {
        const int space = line.indexOf(' ');
        const String path = (space > 0) ? line.substring(space + 1) : "/welcome.wav";
        Serial.println(g_audio.playFile(path.c_str()) ? "OK PLAY" : "ERR PLAY");
        return;
    }
    if (line == "RESET_METRICS") {
        g_audio.resetMetrics();
        Serial.println("OK RESET_METRICS");
        return;
    }
    if (line == "HELP") {
        printHelp();
        return;
    }

    Serial.printf("ERR UNKNOWN_COMMAND %s\n", line.c_str());
}

void pollSerial() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            handleSerialCommand(g_serial_line);
            g_serial_line = "";
        } else {
            g_serial_line += c;
        }
    }
}
}  // namespace

void setup() {
    Serial.begin(kSerialBaud);
    delay(200);

    g_profile = detectBoardProfile();
    g_features = getFeatureMatrix(g_profile);

    const SlicPins slic_pins = slicPinsForProfile(g_profile);
    const bool slic_ok = g_slic.begin(slic_pins);
    g_slic.setPowerDown(false);
    g_slic.setLineEnabled(true);
    g_slic.setRing(false);

    const AudioConfig audio_cfg = defaultAudioConfigForProfile(g_profile);
    const bool audio_ok = g_audio.begin(audio_cfg);
    g_audio.resetMetrics();

    g_telephony.begin(g_profile, g_slic, g_audio);

    g_web.setRateLimitMs(1000);
    g_web.setAuthEnabled(false);
    g_web.setControlCallback(onWebControl);
    g_web.setStatusCallback(onWebStatus);
    g_web.begin();

    Serial.printf("[RTC_BL_PHONE] Boot: profile=%s bt_classic=%s full_duplex=%s slic=%s audio=%s\n",
                  boardProfileToString(g_profile), g_features.has_bt_classic ? "true" : "false",
                  g_features.has_full_duplex_i2s ? "true" : "false", slic_ok ? "ok" : "fail",
                  audio_ok ? "ok" : "fail");
    printHelp();
}

void loop() {
    g_telephony.tick();
    pollSerial();
    delay(10);
}
#endif  // UNIT_TEST
