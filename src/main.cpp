#include <Arduino.h>
#include <ArduinoJson.h>

#include "audio/AudioEngine.h"
#include "audio/Es8388Driver.h"
#include "bluetooth/BluetoothManager.h"
#include "config/A252ConfigStore.h"
#include "core/CommandDispatcher.h"
#include "core/PlatformProfile.h"
#include "props/EspNowBridge.h"
#include "props/PropsBridge.h"
#include "slic/Ks0835SlicController.h"
#include "telephony/TelephonyService.h"
#include "web/WebServerManager.h"
#include "wifi/WifiCredentialsStorage.h"
#include "wifi/WifiManager.h"

#ifndef UNIT_TEST
namespace {

constexpr uint32_t kSerialBaud = 115200;

BoardProfile g_profile = BoardProfile::ESP32_A252;
FeatureMatrix g_features = getFeatureMatrix(BoardProfile::ESP32_A252);

A252PinsConfig g_pins_cfg = A252ConfigStore::defaultPins();
A252AudioConfig g_audio_cfg = A252ConfigStore::defaultAudio();
MqttConfig g_mqtt_cfg = A252ConfigStore::defaultMqtt();
EspNowPeerStore g_peer_store;

Ks0835SlicController g_slic;
AudioEngine g_audio;
Es8388Driver g_codec;
TelephonyService g_telephony;
WifiManager g_wifi;
PropsBridge g_mqtt;
EspNowBridge g_espnow;
BluetoothManager g_bt;
WebServerManager g_web;
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
        .pin_line_enable = static_cast<int8_t>(g_pins_cfg.slic_line),
        .pin_pd = static_cast<int8_t>(g_pins_cfg.slic_pd),
        .hook_active_high = g_pins_cfg.hook_active_high,
    };

    const bool slic_ok = g_slic.begin(slic_pins);
    g_slic.setPowerDown(false);
    g_slic.setLineEnabled(true);
    g_slic.setRing(false);

    const bool codec_ok = g_codec.begin(g_pins_cfg.es8388_sda, g_pins_cfg.es8388_scl);
    g_codec.setVolume(g_audio_cfg.volume);
    g_codec.setMute(g_audio_cfg.mute);
    g_codec.setRoute(g_audio_cfg.route);

    const AudioConfig audio = buildI2sConfig(g_pins_cfg, g_audio_cfg);
    const bool audio_ok = g_audio.begin(audio);
    g_audio.resetMetrics();

    g_telephony.begin(g_profile, g_slic, g_audio);

    Serial.printf("[RTC_BL_PHONE] HW init slic=%s codec=%s audio=%s\n",
                  slic_ok ? "ok" : "fail",
                  codec_ok ? "ok" : "fail",
                  audio_ok ? "ok" : "fail");

    return slic_ok && audio_ok;
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

    JsonObject wifi = root["wifi"].to<JsonObject>();
    g_wifi.statusToJson(wifi);

    JsonObject mqtt = root["mqtt"].to<JsonObject>();
    g_mqtt.statusToJson(mqtt);

    JsonObject espnow = root["espnow"].to<JsonObject>();
    g_espnow.statusToJson(espnow);

    JsonObject bluetooth = root["bluetooth"].to<JsonObject>();
    g_bt.statusToJson(bluetooth);

    JsonObject config = root["config"].to<JsonObject>();
    A252ConfigStore::pinsToJson(g_pins_cfg, config["pins"].to<JsonObject>());
    A252ConfigStore::audioToJson(g_audio_cfg, config["audio"].to<JsonObject>());
    A252ConfigStore::mqttToJson(g_mqtt_cfg, config["mqtt"].to<JsonObject>(), false);

    JsonArray peers = config["espnow_peers"].to<JsonArray>();
    A252ConfigStore::peersToJson(g_peer_store, peers);
}

void publishStatusIfConnected() {
    if (!g_mqtt.isConnected()) {
        return;
    }
    JsonDocument doc;
    fillStatusSnapshot(doc.to<JsonObject>());
    String payload;
    serializeJson(doc, payload);
    g_mqtt.publish("status", payload, false);
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
    if (patch["slic"]["line"].is<int>()) {
        next.slic_line = patch["slic"]["line"].as<int>();
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
    if (patch["slic_line"].is<int>()) {
        next.slic_line = patch["slic_line"].as<int>();
    }
    if (patch["slic_pd"].is<int>()) {
        next.slic_pd = patch["slic_pd"].as<int>();
    }
    if (patch["hook_active_high"].is<bool>()) {
        next.hook_active_high = patch["hook_active_high"].as<bool>();
    }

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

bool applyMqttPatch(JsonVariantConst patch, MqttConfig& target, String& error) {
    MqttConfig next = target;

    if (patch["enabled"].is<bool>()) {
        next.enabled = patch["enabled"].as<bool>();
    }
    if (patch["host"].is<const char*>()) {
        next.host = patch["host"].as<const char*>();
    }
    if (patch["port"].is<uint16_t>()) {
        next.port = patch["port"].as<uint16_t>();
    }
    if (patch["user"].is<const char*>()) {
        next.user = patch["user"].as<const char*>();
    }
    if (patch["pass"].is<const char*>()) {
        next.pass = patch["pass"].as<const char*>();
    }
    if (patch["base_topic"].is<const char*>()) {
        next.base_topic = patch["base_topic"].as<const char*>();
    }

    if (!A252ConfigStore::validateMqtt(next, error)) {
        return false;
    }

    target = next;
    return true;
}

DispatchResponse executeCommandLine(const String& line) {
    return g_dispatcher.dispatch(line);
}

String responseToText(const DispatchResponse& res) {
    if (!res.raw.isEmpty()) {
        return res.raw;
    }
    if (!res.json.isEmpty()) {
        return res.json;
    }
    String out = res.ok ? "OK" : "ERR";
    if (!res.code.isEmpty()) {
        out += " ";
        out += res.code;
    }
    return out;
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

    g_dispatcher.registerCommand("WIFI_CONNECT", [](const String& args) {
        String ssid;
        String password_raw;
        if (!splitFirstToken(args, ssid, password_raw) || ssid.isEmpty()) {
            return makeResponse(false, "WIFI_CONNECT invalid_args");
        }

        String password = password_raw;
        if (!password_raw.isEmpty()) {
            String parsed;
            String remaining;
            if (splitFirstToken(password_raw, parsed, remaining) && remaining.isEmpty()) {
                password = parsed;
            }
        }

        const bool ok = g_wifi.connect(ssid, password, 10000, true);
        return makeResponse(ok, "WIFI_CONNECT");
    });

    g_dispatcher.registerCommand("WIFI_DISCONNECT", [](const String&) {
        g_wifi.disconnect(false);
        return makeResponse(true, "WIFI_DISCONNECT");
    });

    g_dispatcher.registerCommand("WIFI_STATUS", [](const String&) {
        JsonDocument doc;
        g_wifi.statusToJson(doc.to<JsonObject>());
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WIFI_SCAN", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        root["ok"] = true;
        JsonArray nets = root["networks"].to<JsonArray>();
        g_wifi.scanToJson(nets);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("WIFI_RECONNECT", [](const String&) {
        return makeResponse(g_wifi.reconnect(10000), "WIFI_RECONNECT");
    });

    g_dispatcher.registerCommand("MQTT_CONFIG_SET", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "MQTT_CONFIG_SET invalid_json");
        }
        JsonDocument doc;
        if (deserializeJson(doc, args) != DeserializationError::Ok) {
            return makeResponse(false, "MQTT_CONFIG_SET invalid_json");
        }

        MqttConfig next = g_mqtt_cfg;
        String error;
        if (!applyMqttPatch(doc.as<JsonVariantConst>(), next, error)) {
            return makeResponse(false, "MQTT_CONFIG_SET " + error);
        }
        if (!A252ConfigStore::saveMqtt(next, &error)) {
            return makeResponse(false, "MQTT_CONFIG_SET " + error);
        }

        g_mqtt_cfg = next;
        g_mqtt.setConfig(g_mqtt_cfg);

        JsonDocument out;
        A252ConfigStore::mqttToJson(g_mqtt_cfg, out.to<JsonObject>(), false);
        return jsonResponse(out);
    });

    g_dispatcher.registerCommand("MQTT_CONNECT", [](const String&) {
        if (!g_mqtt_cfg.enabled) {
            return makeResponse(false, "MQTT_CONNECT disabled");
        }
        return makeResponse(g_mqtt.connectNow(), "MQTT_CONNECT");
    });

    g_dispatcher.registerCommand("MQTT_DISCONNECT", [](const String&) {
        g_mqtt.disconnect();
        return makeResponse(true, "MQTT_DISCONNECT");
    });

    g_dispatcher.registerCommand("MQTT_STATUS", [](const String&) {
        JsonDocument doc;
        JsonObject root = doc.to<JsonObject>();
        g_mqtt.statusToJson(root);
        A252ConfigStore::mqttToJson(g_mqtt_cfg, root["config"].to<JsonObject>(), false);
        return jsonResponse(doc);
    });

    g_dispatcher.registerCommand("MQTT_PUB", [](const String& args) {
        String topic;
        String payload;
        if (!splitFirstToken(args, topic, payload) || topic.isEmpty()) {
            return makeResponse(false, "MQTT_PUB invalid_args");
        }
        const bool ok = g_mqtt.publishRaw(topic, payload, false);
        return makeResponse(ok, "MQTT_PUB");
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

    g_dispatcher.registerCommand("BT_HFP_CONNECT", [](const String& args) {
        if (args.isEmpty()) {
            return makeResponse(false, "BT_HFP_CONNECT invalid_addr");
        }
        const bool ok = g_bt.connect(args.c_str()) && g_bt.startHFP();
        return makeResponse(ok, "BT_HFP_CONNECT");
    });

    g_dispatcher.registerCommand("BT_HFP_DISCONNECT", [](const String&) {
        g_bt.stopHFP();
        const bool ok = g_bt.disconnect();
        return makeResponse(ok, "BT_HFP_DISCONNECT");
    });

    g_dispatcher.registerCommand("BT_DIAL", [](const String& args) {
        String number = args;
        number.trim();
        if (number.isEmpty()) {
            return makeResponse(false, "BT_DIAL invalid_number");
        }
        return makeResponse(g_bt.dial(number), "BT_DIAL");
    });

    g_dispatcher.registerCommand("DIAL", [](const String& args) {
        String number = args;
        number.trim();
        if (number.isEmpty()) {
            return makeResponse(false, "DIAL invalid_number");
        }
        return makeResponse(g_bt.dial(number), "DIAL");
    });

    g_dispatcher.registerCommand("BT_REDIAL", [](const String&) {
        return makeResponse(g_bt.redial(), "BT_REDIAL");
    });

    g_dispatcher.registerCommand("BT_ANSWER", [](const String&) {
        return makeResponse(g_bt.answerCall(), "BT_ANSWER");
    });

    g_dispatcher.registerCommand("BT_HANGUP", [](const String&) {
        return makeResponse(g_bt.hangupCall(), "BT_HANGUP");
    });

    g_dispatcher.registerCommand("BT_CALLS", [](const String&) {
        return makeResponse(g_bt.queryCurrentCalls(), "BT_CALLS");
    });

    g_dispatcher.registerCommand("BT_PBAP_SYNC", [](const String&) {
        return makeResponse(g_bt.syncPbapContacts(), "BT_PBAP_SYNC unsupported");
    });

    g_dispatcher.registerCommand("BT_BLE_START", [](const String&) {
        return makeResponse(g_bt.startBLE(), "BT_BLE_START");
    });

    g_dispatcher.registerCommand("BT_BLE_STOP", [](const String&) {
        return makeResponse(g_bt.stopBLE(), "BT_BLE_STOP");
    });

    g_dispatcher.registerCommand("BT_DISCOVERABLE_ON", [](const String&) {
        return makeResponse(g_bt.setDiscoverable(true), "BT_DISCOVERABLE_ON");
    });

    g_dispatcher.registerCommand("BT_DISCOVERABLE_OFF", [](const String&) {
        return makeResponse(g_bt.setDiscoverable(false), "BT_DISCOVERABLE_OFF");
    });

    g_dispatcher.registerCommand("BT_STATUS", [](const String&) {
        JsonDocument doc;
        g_bt.statusToJson(doc.to<JsonObject>());
        return jsonResponse(doc);
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
    if (!extractBridgeCommand(payload, cmd)) {
        return;
    }

    const DispatchResponse result = executeCommandLine(cmd);

    JsonDocument event;
    event["source"] = source;
    event["cmd"] = cmd;
    event["ok"] = result.ok;
    event["code"] = result.code;
    String payload_json;
    serializeJson(event, payload_json);
    g_mqtt.publish("event", payload_json, false);
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
    delay(200);

    g_profile = BoardProfile::ESP32_A252;
    g_features = getFeatureMatrix(g_profile);

    A252ConfigStore::loadPins(g_pins_cfg);
    A252ConfigStore::loadAudio(g_audio_cfg);
    A252ConfigStore::loadMqtt(g_mqtt_cfg);
    A252ConfigStore::loadEspNowPeers(g_peer_store);

    applyHardwareConfig();
    registerCommands();

    g_bt.setBleCommandHandler([](const String& cmd) {
        return responseToText(executeCommandLine(cmd));
    });

    g_bt.begin(g_profile);

    g_mqtt.begin(g_mqtt_cfg);
    g_mqtt.setCommandCallback([](const String& source, const JsonVariantConst& payload) {
        processInboundBridgeCommand(source, payload);
    });

    g_espnow.begin(g_peer_store);
    g_espnow.setCommandCallback([](const String& source, const JsonVariantConst& payload) {
        processInboundBridgeCommand(source, payload);
    });

    String ssid;
    String password;
    if (WifiCredentialsStorage::load(ssid, password)) {
        g_wifi.connect(ssid, password, 10000, false);
        if (g_wifi.isConnected() && g_mqtt_cfg.enabled) {
            g_mqtt.connectNow();
        }
    } else {
        g_wifi.ensureFallbackAp();
    }

    g_web.setRateLimitMs(250);
    g_web.setAuthEnabled(false);
    g_web.setStatusCallback(fillStatusSnapshot);
    g_web.setCommandExecutor(executeCommandLine);
    g_web.begin();

    Serial.printf("[RTC_BL_PHONE] Boot: profile=%s bt_classic=%s full_duplex=%s\n",
                  boardProfileToString(g_profile),
                  g_features.has_bt_classic ? "true" : "false",
                  g_features.has_full_duplex_i2s ? "true" : "false");
    printHelp();
    publishStatusIfConnected();
}

void loop() {
    g_telephony.tick();
    g_wifi.loop();
    g_mqtt.tick();
    g_espnow.tick();
    pollSerial();
    delay(10);
}
#endif  // UNIT_TEST
