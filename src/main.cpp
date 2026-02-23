#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <WiFi.h>

#include "audio/AudioEngine.h"
#include "audio/Es8388Driver.h"
#include "config/A252ConfigStore.h"
#include "core/CommandDispatcher.h"
#include "core/PlatformProfile.h"
#include "props/EspNowBridge.h"
#include "slic/Ks0835SlicController.h"
#include "telephony/TelephonyService.h"

#ifndef UNIT_TEST
namespace {

constexpr uint32_t kSerialBaud = 115200;
constexpr int kAudioAmpEnablePin = 21;
constexpr char kBootLogTag[] = "RTC_BOOT";
constexpr bool kPrintHelpOnBoot = false;

BoardProfile g_profile = BoardProfile::ESP32_A252;
FeatureMatrix g_features = getFeatureMatrix(BoardProfile::ESP32_A252);

A252PinsConfig g_pins_cfg = A252ConfigStore::defaultPins();
A252AudioConfig g_audio_cfg = A252ConfigStore::defaultAudio();
EspNowPeerStore g_peer_store;

Ks0835SlicController g_slic;
AudioEngine g_audio;
Es8388Driver g_codec;
TelephonyService g_telephony;
EspNowBridge g_espnow;
CommandDispatcher g_dispatcher;
String g_serial_line;

DispatchResponse makeResponse(bool ok, const String& code) {
    DispatchResponse res;
    res.ok = ok;
    res.code = code;
    return res;
}

DispatchResponse jsonResponse(JsonDocument& doc) {
    DispatchResponse res;
    res.ok = true;
    serializeJson(doc, res.json);
    return res;
}

bool splitFirstToken(const String& input, String& first, String& rest) {
    String work = input;
    work.trim();
    if (work.isEmpty()) {
        first = "";
        rest = "";
        return false;
    }

    if (work[0] == '"') {
        bool escaped = false;
        int close_index = -1;
        for (int i = 1; i < work.length(); ++i) {
            const char c = work[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (c == '"') {
                close_index = i;
                break;
            }
        }
        if (close_index < 0) {
            first = "";
            rest = "";
            return false;
        }

        String token = work.substring(1, close_index);
        token.replace("\\\"", "\"");
        token.replace("\\\\", "\\");
        first = token;
        rest = work.substring(close_index + 1);
        rest.trim();
        return true;
    }

    const int sep = work.indexOf(' ');
    if (sep < 0) {
        first = work;
        rest = "";
        return true;
    }

    first = work.substring(0, sep);
    rest = work.substring(sep + 1);
    rest.trim();
    return true;
}

bool extractBridgeCommand(JsonVariantConst payload, String& out_cmd, uint8_t depth = 0) {
    if (depth > 4U) {
        return false;
    }

    if (payload.is<const char*>()) {
        out_cmd = payload.as<const char*>();
        out_cmd.trim();
        return !out_cmd.isEmpty();
    }

    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    const char* keys[] = {"cmd", "raw", "command", "action"};
    for (const char* key : keys) {
        if (!payload[key].is<const char*>()) {
            continue;
        }
        out_cmd = payload[key].as<const char*>();
        out_cmd.trim();
        if (!out_cmd.isEmpty()) {
            return true;
        }
    }

    if (!payload["event"].isNull() && extractBridgeCommand(payload["event"], out_cmd, static_cast<uint8_t>(depth + 1U))) {
        return true;
    }
    if (!payload["message"].isNull() &&
        extractBridgeCommand(payload["message"], out_cmd, static_cast<uint8_t>(depth + 1U))) {
        return true;
    }
    if (!payload["payload"].isNull() &&
        extractBridgeCommand(payload["payload"], out_cmd, static_cast<uint8_t>(depth + 1U))) {
        return true;
    }

    return false;
}

bool buildEspNowEnvelopeCommand(JsonVariantConst payload,
                                String& out_cmd,
                                String& out_msg_id,
                                uint32_t& out_seq,
                                bool& out_ack_requested) {
    out_cmd = "";
    out_msg_id = "";
    out_seq = 0;
    out_ack_requested = true;

    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    JsonObjectConst obj = payload.as<JsonObjectConst>();
    if (!obj["type"].is<const char*>()) {
        return false;
    }

    String type = obj["type"] | "";
    type.toLowerCase();
    if (type != "command" && type != "request" && type != "cmd") {
        return false;
    }

    out_msg_id = obj["msg_id"] | "";
    out_seq = obj["seq"] | 0;
    out_ack_requested = obj["ack"] | true;

    JsonVariantConst body = obj["payload"];
    if (body.isNull()) {
        return false;
    }

    if (body.is<const char*>()) {
        out_cmd = body.as<const char*>();
        out_cmd.trim();
        return !out_cmd.isEmpty();
    }

    if (body.is<JsonObjectConst>()) {
        JsonObjectConst body_obj = body.as<JsonObjectConst>();
        const String cmd = body_obj["cmd"] | "";
        if (!cmd.isEmpty()) {
            out_cmd = cmd;
            out_cmd.trim();
            if (out_cmd.isEmpty()) {
                return false;
            }

            if (!body_obj["args"].isNull()) {
                String args;
                serializeJson(body_obj["args"], args);
                args.trim();
                if (!args.isEmpty() && args != "null") {
                    out_cmd += " ";
                    out_cmd += args;
                }
            }
            return true;
        }
    }

    return extractBridgeCommand(body, out_cmd);
}

bool buildRtcBlV1BridgeCommand(JsonVariantConst payload,
                               String& out_cmd,
                               String& out_request_id,
                               bool& out_is_v1) {
    out_is_v1 = false;
    if (!payload.is<JsonObjectConst>()) {
        return false;
    }

    JsonObjectConst obj = payload.as<JsonObjectConst>();
    const String proto = obj["proto"] | "";
    if (!proto.equalsIgnoreCase("rtcbl/1")) {
        return false;
    }

    const String cmd = obj["cmd"] | "";
    if (cmd.isEmpty()) {
        return false;
    }

    out_cmd = cmd;
    out_cmd.trim();
    if (out_cmd.isEmpty()) {
        return false;
    }

    out_request_id = obj["id"] | "";
    out_is_v1 = true;

    if (obj["args"].isNull()) {
        return true;
    }

    String args;
    serializeJson(obj["args"], args);
    args.trim();
    if (!args.isEmpty() && args != "null") {
        out_cmd += " ";
        out_cmd += args;
    }

    return true;
}

bool isMacAddressString(const String& value) {
    uint8_t mac[6] = {0};
    return A252ConfigStore::parseMac(value, mac);
}

AudioConfig buildI2sConfig(const A252PinsConfig& pins_cfg, const A252AudioConfig& audio_cfg) {
    AudioConfig cfg;
    cfg.port = I2S_NUM_0;
    cfg.sample_rate = audio_cfg.sample_rate;
    cfg.bits_per_sample = (audio_cfg.bits_per_sample == 32)
                              ? I2S_BITS_PER_SAMPLE_32BIT
                              : (audio_cfg.bits_per_sample == 24) ? I2S_BITS_PER_SAMPLE_24BIT
                                                                    : I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.bck_pin = pins_cfg.i2s_bck;
    cfg.ws_pin = pins_cfg.i2s_ws;
    cfg.data_out_pin = pins_cfg.i2s_dout;
    cfg.data_in_pin = pins_cfg.i2s_din;
    cfg.enable_capture = audio_cfg.enable_capture;
    cfg.dma_buf_count = 8;
    cfg.dma_buf_len = 256;
    return cfg;
}

bool applyHardwareConfig() {
    const SlicPins slic_pins = {
        .pin_rm = static_cast<uint8_t>(g_pins_cfg.slic_rm),
        .pin_fr = static_cast<uint8_t>(g_pins_cfg.slic_fr),
        .pin_shk = static_cast<uint8_t>(g_pins_cfg.slic_shk),
        .pin_line_enable = static_cast<int8_t>(-1),
        .pin_pd = static_cast<int8_t>(g_pins_cfg.slic_pd),
        .hook_active_high = g_pins_cfg.hook_active_high,
    };

    const bool slic_ok = g_slic.begin(slic_pins);
    g_slic.setPowerDown(false);
    g_slic.setRing(false);

    const bool codec_ok = g_codec.begin(g_pins_cfg.es8388_sda, g_pins_cfg.es8388_scl);
    g_codec.setVolume(g_audio_cfg.volume);
    g_codec.setMute(g_audio_cfg.mute);
    g_codec.setRoute(g_audio_cfg.route);

    const AudioConfig audio = buildI2sConfig(g_pins_cfg, g_audio_cfg);
    const bool audio_ok = g_audio.begin(audio);
    g_audio.resetMetrics();

    g_telephony.begin(g_profile, g_slic, g_audio);
    g_telephony.setDialCallback([](const String& number) {
        Serial.printf("[Telephony] dial request ignored (BT disabled): %s\n", number.c_str());
        return false;
    });
    g_telephony.setAnswerCallback([]() {
        Serial.println("[Telephony] answer request ignored (BT disabled)");
        return false;
    });

    Serial.printf("[RTC_BL_PHONE] HW init slic=%s codec=%s audio=%s\n",
                  slic_ok ? "ok" : "fail",
                  codec_ok ? "ok" : "fail",
                  audio_ok ? "ok" : "fail");

    return slic_ok && codec_ok && audio_ok;
}

void appendAudioMetrics(JsonObject root) {
    const AudioRuntimeMetrics metrics = g_audio.metrics();

    root["audio_frames_requested"] = metrics.frames_requested;
    root["audio_frames_read"] = metrics.frames_read;
    root["audio_drop_frames"] = metrics.drop_frames;
    root["audio_underrun_count"] = metrics.underrun_count;
    root["audio_last_latency_ms"] = metrics.last_latency_ms;
    root["audio_max_latency_ms"] = metrics.max_latency_ms;

    JsonObject audio = root["audio"].to<JsonObject>();
    audio["full_duplex"] = g_audio.supportsFullDuplex();
    audio["dial_tone_active"] = g_audio.isDialToneActive();
    audio["playing"] = g_audio.isPlaying();
    audio["sd_ready"] = g_audio.isSdReady();
    audio["frames"] = metrics.frames_read;
    audio["underrun"] = metrics.underrun_count;
    audio["drop"] = metrics.drop_frames;
    audio["latence_ms"] = metrics.last_latency_ms;
}

void fillStatusSnapshot(JsonObject root) {
    root["board_profile"] = boardProfileToString(g_profile);

    JsonObject telephony = root["telephony"].to<JsonObject>();
    telephony["state"] = telephonyStateToString(g_telephony.state());
    telephony["hook"] = g_slic.isHookOff() ? "OFF_HOOK" : "ON_HOOK";

    appendAudioMetrics(root);

    JsonObject espnow = root["espnow"].to<JsonObject>();
    g_espnow.statusToJson(espnow);

    JsonObject config = root["config"].to<JsonObject>();
    A252ConfigStore::pinsToJson(g_pins_cfg, config["pins"].to<JsonObject>());
    A252ConfigStore::audioToJson(g_audio_cfg, config["audio"].to<JsonObject>());

    JsonArray peers = config["espnow_peers"].to<JsonArray>();
    A252ConfigStore::peersToJson(g_peer_store, peers);
}

bool applyPinsPatch(JsonVariantConst patch, A252PinsConfig& target, String& error) {
    A252PinsConfig next = target;

    if (patch["i2s"]["bck"].is<int>()) {
        next.i2s_bck = patch["i2s"]["bck"].as<int>();
    }
    if (patch["i2s"]["ws"].is<int>()) {
        next.i2s_ws = patch["i2s"]["ws"].as<int>();
    }
    if (patch["i2s"]["dout"].is<int>()) {
        next.i2s_dout = patch["i2s"]["dout"].as<int>();
    }
    if (patch["i2s"]["din"].is<int>()) {
        next.i2s_din = patch["i2s"]["din"].as<int>();
    }

    if (patch["codec_i2c"]["sda"].is<int>()) {
        next.es8388_sda = patch["codec_i2c"]["sda"].as<int>();
    }
    if (patch["codec_i2c"]["scl"].is<int>()) {
        next.es8388_scl = patch["codec_i2c"]["scl"].as<int>();
    }

    if (patch["slic"]["rm"].is<int>()) {
        next.slic_rm = patch["slic"]["rm"].as<int>();
    }
    if (patch["slic"]["fr"].is<int>()) {
        next.slic_fr = patch["slic"]["fr"].as<int>();
    }
    if (patch["slic"]["shk"].is<int>()) {
        next.slic_shk = patch["slic"]["shk"].as<int>();
    }
    if (patch["slic"]["pd"].is<int>()) {
        next.slic_pd = patch["slic"]["pd"].as<int>();
    }
    if (patch["slic"]["hook_active_high"].is<bool>()) {
        next.hook_active_high = patch["slic"]["hook_active_high"].as<bool>();
    }

    if (patch["i2s_bck"].is<int>()) {
        next.i2s_bck = patch["i2s_bck"].as<int>();
    }
    if (patch["i2s_ws"].is<int>()) {
        next.i2s_ws = patch["i2s_ws"].as<int>();
    }
    if (patch["i2s_dout"].is<int>()) {
        next.i2s_dout = patch["i2s_dout"].as<int>();
    }
    if (patch["i2s_din"].is<int>()) {
        next.i2s_din = patch["i2s_din"].as<int>();
    }

    if (patch["es8388_sda"].is<int>()) {
        next.es8388_sda = patch["es8388_sda"].as<int>();
    }
    if (patch["es8388_scl"].is<int>()) {
        next.es8388_scl = patch["es8388_scl"].as<int>();
    }

    if (patch["slic_rm"].is<int>()) {
        next.slic_rm = patch["slic_rm"].as<int>();
    }
    if (patch["slic_fr"].is<int>()) {
        next.slic_fr = patch["slic_fr"].as<int>();
    }
    if (patch["slic_shk"].is<int>()) {
        next.slic_shk = patch["slic_shk"].as<int>();
    }
    if (patch["slic_pd"].is<int>()) {
        next.slic_pd = patch["slic_pd"].as<int>();
    }
    if (patch["hook_active_high"].is<bool>()) {
        next.hook_active_high = patch["hook_active_high"].as<bool>();
    }

    next.slic_line = -1;

    if (!A252ConfigStore::validatePins(next, error)) {
        return false;
    }

    target = next;
    return true;
}

bool applyAudioPatch(JsonVariantConst patch, A252AudioConfig& target, String& error) {
    A252AudioConfig next = target;

    if (patch["sample_rate"].is<uint32_t>()) {
        next.sample_rate = patch["sample_rate"].as<uint32_t>();
    }
    if (patch["bits_per_sample"].is<uint8_t>()) {
        next.bits_per_sample = patch["bits_per_sample"].as<uint8_t>();
    }
    if (patch["enable_capture"].is<bool>()) {
        next.enable_capture = patch["enable_capture"].as<bool>();
    }
    if (patch["volume"].is<uint8_t>()) {
        next.volume = patch["volume"].as<uint8_t>();
    }
    if (patch["mute"].is<bool>()) {
        next.mute = patch["mute"].as<bool>();
    }
    if (patch["route"].is<const char*>()) {
        next.route = patch["route"].as<const char*>();
        next.route.toLowerCase();
    }

    if (!A252ConfigStore::validateAudio(next, error)) {
        return false;
    }
    target = next;
    return true;
}

DispatchResponse executeCommandLine(const String& line) {
    return g_dispatcher.dispatch(line);
}

void registerCommands() {
    g_dispatcher.registerCommand("PING", [](const String&) {
        DispatchResponse res;
        res.ok = true;
        res.raw = "PONG";
        return res;
    });

    g_dispatcher.registerCommand("HELP", [](const String&) {
        DispatchResponse res;
        res.ok = true;
        res.raw = g_dispatcher.helpText();
        return res;
    });

    g_dispatcher.registerCommand("STATUS", [](const String&) {
        JsonDocument doc;
        fillStatusSnapshot(doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("CALL", [](const String&) {
        g_telephony.triggerIncomingRing();
        return makeResponse(true, "CALL");
    });

    g_dispatcher.registerCommand("RING", [](const String&) {
        g_telephony.triggerIncomingRing();
        return makeResponse(true, "RING");
    });

    g_dispatcher.registerCommand("WIFI_STATUS", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        const wl_status_t status = WiFi.status();
        const bool connected = status == WL_CONNECTED;

        root["connected"] = connected;
        root["status"] = status;
        if (connected) {
            root["ip"] = WiFi.localIP().toString();
            root["rssi"] = WiFi.RSSI();
            root["channel"] = WiFi.channel();
        } else {
            root["ip"] = "";
            root["rssi"] = 0;
            root["channel"] = 0;
        }
        root["mode"] = WiFi.getMode() == WIFI_MODE_STA
                           ? "STA"
                           : WiFi.getMode() == WIFI_MODE_AP
                                 ? "AP"
                                 : WiFi.getMode() == WIFI_MODE_APSTA
                                       ? "APSTA"
                                       : "NULL";
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WIFI_DISCONNECT", [](const String&) {
        return makeResponse(false, "unsupported_command WIFI_DISCONNECT");
    });

    g_dispatcher.registerCommand("WIFI_RECONNECT", [](const String&) {
        return makeResponse(false, "unsupported_command WIFI_RECONNECT");
    });

    g_dispatcher.registerCommand("UNLOCK", [](const String&) {
        return makeResponse(false, "unsupported_command UNLOCK");
    });

    g_dispatcher.registerCommand("NEXT", [](const String&) {
        return makeResponse(false, "unsupported_command NEXT");
    });

    g_dispatcher.registerCommand("STORY_REFRESH_SD", [](const String&) {
        return makeResponse(false, "unsupported_command STORY_REFRESH_SD");
    });

    g_dispatcher.registerCommand("SC_EVENT", [](const String&) {
        return makeResponse(false, "unsupported_command SC_EVENT");
    });

    g_dispatcher.registerCommand("SCENE", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "missing_scene_id");
        }
        return makeResponse(false, "scene_not_found");
    });

    g_dispatcher.registerCommand("CAPTURE_START", [](const String&) {
        return makeResponse(g_audio.startCapture(), "CAPTURE_START");
    });

    g_dispatcher.registerCommand("CAPTURE_STOP", [](const String&) {
        g_audio.stopCapture();
        return makeResponse(true, "CAPTURE_STOP");
    });

    g_dispatcher.registerCommand("PLAY", [](const String& args) {
        const String path = args.isEmpty() ? "/welcome.wav" : args;
        return makeResponse(g_audio.playFile(path.c_str()), "PLAY");
    });

    g_dispatcher.registerCommand("RESET_METRICS", [](const String&) {
        g_audio.resetMetrics();
        return makeResponse(true, "RESET_METRICS");
    });

    g_dispatcher.registerCommand("TONE_ON", [](const String&) {
        return makeResponse(g_audio.startDialTone(), "TONE_ON");
    });

    g_dispatcher.registerCommand("TONE_OFF", [](const String&) {
        g_audio.stopDialTone();
        return makeResponse(true, "TONE_OFF");
    });

    g_dispatcher.registerCommand("AMP_ON", [](const String&) {
        // Locked polarity for A252 bench: AMP_EN active LOW on GPIO21.
        digitalWrite(kAudioAmpEnablePin, LOW);
        return makeResponse(true, "AMP_ON");
    });

    g_dispatcher.registerCommand("AMP_OFF", [](const String&) {
        digitalWrite(kAudioAmpEnablePin, HIGH);
        return makeResponse(true, "AMP_OFF");
    });

    g_dispatcher.registerCommand("ESPNOW_ON", [](const String&) {
        return makeResponse(g_espnow.begin(g_peer_store), "ESPNOW_ON");
    });

    g_dispatcher.registerCommand("ESPNOW_OFF", [](const String&) {
        return makeResponse(g_espnow.stop(), "ESPNOW_OFF");
    });

    g_dispatcher.registerCommand("ESPNOW_PEER_ADD", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "ESPNOW_PEER_ADD invalid_mac");
        }
        const bool ok = g_espnow.addPeer(args);
        if (ok) {
            g_peer_store.peers = g_espnow.peers();
            A252ConfigStore::saveEspNowPeers(g_peer_store);
        }
        return makeResponse(ok, "ESPNOW_PEER_ADD");
    });

    g_dispatcher.registerCommand("ESPNOW_PEER_DEL", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "ESPNOW_PEER_DEL invalid_mac");
        }
        const bool ok = g_espnow.deletePeer(args);
        if (ok) {
            g_peer_store.peers = g_espnow.peers();
            A252ConfigStore::saveEspNowPeers(g_peer_store);
        }
        return makeResponse(ok, "ESPNOW_PEER_DEL");
    });

    g_dispatcher.registerCommand("ESPNOW_PEER_LIST", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        JsonArray peers = root["peers"].to<JsonArray>();
        g_peer_store.peers = g_espnow.peers();
        A252ConfigStore::peersToJson(g_peer_store, peers);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_STATUS", [](const String&) {
        JsonDocument doc;
        g_espnow.statusToJson(doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("ESPNOW_SEND", [](const String& args) {
        String target;
        String payload;
        if (!splitFirstToken(args, target, payload) || target.isEmpty() || payload.isEmpty()) {
            return makeResponse(false, "ESPNOW_SEND invalid_args");
        }
        return makeResponse(g_espnow.sendJson(target, payload), "ESPNOW_SEND");
    });

    g_dispatcher.registerCommand("SLIC_CONFIG_GET", [](const String&) {
        JsonDocument doc;
        A252ConfigStore::pinsToJson(g_pins_cfg, doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("SLIC_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "SLIC_CONFIG_SET invalid_json");
        }

        JsonDocument doc;
        if (deserializeJson(doc, args) != DeserializationError::Ok) {
            return makeResponse(false, "SLIC_CONFIG_SET invalid_json");
        }

        A252PinsConfig next = g_pins_cfg;
        String error;
        if (!applyPinsPatch(doc.as<JsonVariantConst>(), next, error)) {
            return makeResponse(false, "SLIC_CONFIG_SET " + error);
        }
        if (!A252ConfigStore::savePins(next, &error)) {
            return makeResponse(false, "SLIC_CONFIG_SET " + error);
        }

        const A252PinsConfig prev = g_pins_cfg;
        g_pins_cfg = next;
        if (!applyHardwareConfig()) {
            g_pins_cfg = prev;
            applyHardwareConfig();
            return makeResponse(false, "SLIC_CONFIG_SET apply_failed");
        }

        JsonDocument out;
        A252ConfigStore::pinsToJson(g_pins_cfg, out.to<JsonObject>());
        return jsonResponse(out);
    });

    g_dispatcher.registerCommand("AUDIO_CONFIG_GET", [](const String&) {
        JsonDocument doc;
        A252ConfigStore::audioToJson(g_audio_cfg, doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("AUDIO_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "AUDIO_CONFIG_SET invalid_json");
        }

        JsonDocument doc;
        if (deserializeJson(doc, args) != DeserializationError::Ok) {
            return makeResponse(false, "AUDIO_CONFIG_SET invalid_json");
        }

        A252AudioConfig next = g_audio_cfg;
        String error;
        if (!applyAudioPatch(doc.as<JsonVariantConst>(), next, error)) {
            return makeResponse(false, "AUDIO_CONFIG_SET " + error);
        }
        if (!A252ConfigStore::saveAudio(next, &error)) {
            return makeResponse(false, "AUDIO_CONFIG_SET " + error);
        }

        g_audio_cfg = next;
        g_codec.setVolume(g_audio_cfg.volume);
        g_codec.setMute(g_audio_cfg.mute);
        g_codec.setRoute(g_audio_cfg.route);
        const bool audio_ok = g_audio.begin(buildI2sConfig(g_pins_cfg, g_audio_cfg));
        return makeResponse(audio_ok, "AUDIO_CONFIG_SET");
    });
}

void processInboundBridgeCommand(const String& source, const JsonVariantConst& payload) {
    String cmd;
    String request_id;
    uint32_t request_seq = 0;
    bool request_ack = true;
    bool is_envelope_v2 = false;
    bool is_rtcbl_v1 = false;

    if (buildEspNowEnvelopeCommand(payload, cmd, request_id, request_seq, request_ack)) {
        is_envelope_v2 = true;
    } else if (!buildRtcBlV1BridgeCommand(payload, cmd, request_id, is_rtcbl_v1) &&
               !extractBridgeCommand(payload, cmd)) {
        return;
    }

    const DispatchResponse result = executeCommandLine(cmd);

    if (is_envelope_v2 && request_ack && isMacAddressString(source)) {
        JsonDocument response;
        response["msg_id"] = request_id.isEmpty() ? String(millis()) : request_id;
        response["seq"] = request_seq;
        response["type"] = "ack";
        response["ack"] = true;

        JsonObject ack_payload = response["payload"].to<JsonObject>();
        ack_payload["ok"] = result.ok;
        ack_payload["code"] = result.code;
        ack_payload["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

        if (!result.json.isEmpty()) {
            JsonDocument parsed;
            if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
                ack_payload["data"].set(parsed.as<JsonVariantConst>());
            } else {
                ack_payload["data_raw"] = result.json;
            }
        } else if (!result.raw.isEmpty()) {
            ack_payload["data_raw"] = result.raw;
        }

        String response_payload;
        serializeJson(response, response_payload);
        g_espnow.sendJson(source, response_payload);
        return;
    }

    if (!is_rtcbl_v1 || !isMacAddressString(source)) {
        return;
    }

    JsonDocument response;
    response["proto"] = "rtcbl/1";
    response["id"] = request_id;
    response["ok"] = result.ok;
    response["code"] = result.code;
    response["error"] = result.ok ? "" : (result.code.isEmpty() ? result.raw : result.code);

    if (!result.json.isEmpty()) {
        JsonDocument parsed;
        if (deserializeJson(parsed, result.json) == DeserializationError::Ok) {
            JsonVariant data = response["data"];
            data.set(parsed.as<JsonVariantConst>());
        } else {
            response["data_raw"] = result.json;
        }
    } else if (!result.raw.isEmpty()) {
        response["data_raw"] = result.raw;
    }

    String response_payload;
    serializeJson(response, response_payload);
    g_espnow.sendJson(source, response_payload);
}

void printHelp() {
    Serial.println("[RTC_BL_PHONE] Commands:");
    const std::vector<String> names = g_dispatcher.commands();
    for (const String& name : names) {
        Serial.printf("  %s\n", name.c_str());
    }
}

void handleSerialCommand(const String& line) {
    const DispatchResponse res = executeCommandLine(line);

    if (!res.raw.isEmpty()) {
        Serial.println(res.raw);
        return;
    }

    if (!res.json.isEmpty()) {
        Serial.println(res.json);
        return;
    }

    Serial.printf("%s %s\n", res.ok ? "OK" : "ERR", res.code.c_str());
}

void pollSerial() {
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (!g_serial_line.isEmpty()) {
                handleSerialCommand(g_serial_line);
                g_serial_line = "";
            }
        } else {
            g_serial_line += c;
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(kSerialBaud);
    delay(80);

    // Warm up ESP-IDF log/stdout locks from the main task context.
    ESP_LOGI(kBootLogTag, "log lock warmup");
    printf("[RTC_BL_PHONE] stdio lock warmup\n");
    fflush(stdout);

    g_profile = BoardProfile::ESP32_A252;
    g_features = getFeatureMatrix(g_profile);

    A252ConfigStore::loadPins(g_pins_cfg);
    g_pins_cfg.slic_line = -1;
    A252ConfigStore::loadAudio(g_audio_cfg);
    A252ConfigStore::loadEspNowPeers(g_peer_store);

    pinMode(kAudioAmpEnablePin, OUTPUT);
    digitalWrite(kAudioAmpEnablePin, LOW);

    applyHardwareConfig();
    registerCommands();

    g_espnow.begin(g_peer_store);
    g_espnow.setCommandCallback([](const String& source, const JsonVariantConst& payload) {
        processInboundBridgeCommand(source, payload);
    });

    Serial.printf("[RTC_BL_PHONE] Boot: profile=%s full_duplex=%s\n",
                  boardProfileToString(g_profile),
                  g_features.has_full_duplex_i2s ? "true" : "false");
    if (kPrintHelpOnBoot) {
        printHelp();
    }
}

void loop() {
    g_telephony.tick();
    g_espnow.tick();
    pollSerial();
    delay(1);
}
#endif  // UNIT_TEST
