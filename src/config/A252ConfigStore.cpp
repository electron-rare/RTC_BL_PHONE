#include "config/A252ConfigStore.h"

#include <Preferences.h>

#include <algorithm>

namespace {
constexpr const char* kPinsNs = "a252-pins";
constexpr const char* kAudioNs = "a252-audio";
constexpr const char* kMqttNs = "mqtt";
constexpr const char* kEspNowNs = "espnow";

bool saveString(Preferences& prefs, const char* key, const String& value) {
    return prefs.putString(key, value) >= 0;
}

bool loadJsonArray(const String& raw, JsonDocument& doc) {
    if (raw.isEmpty()) {
        doc.to<JsonArray>();
        return true;
    }
    const auto err = deserializeJson(doc, raw);
    return err == DeserializationError::Ok && doc.is<JsonArray>();
}

}  // namespace

A252PinsConfig A252ConfigStore::defaultPins() {
    return A252PinsConfig{};
}

A252AudioConfig A252ConfigStore::defaultAudio() {
    return A252AudioConfig{};
}

MqttConfig A252ConfigStore::defaultMqtt() {
    return MqttConfig{};
}

bool A252ConfigStore::loadPins(A252PinsConfig& out) {
    out = defaultPins();
    Preferences prefs;
    if (!prefs.begin(kPinsNs, false)) {
        return false;
    }

    out.i2s_bck = prefs.getInt("i2s_bck", out.i2s_bck);
    out.i2s_ws = prefs.getInt("i2s_ws", out.i2s_ws);
    out.i2s_dout = prefs.getInt("i2s_dout", out.i2s_dout);
    out.i2s_din = prefs.getInt("i2s_din", out.i2s_din);

    out.es8388_sda = prefs.getInt("i2c_sda", out.es8388_sda);
    out.es8388_scl = prefs.getInt("i2c_scl", out.es8388_scl);

    out.slic_rm = prefs.getInt("slic_rm", out.slic_rm);
    out.slic_fr = prefs.getInt("slic_fr", out.slic_fr);
    out.slic_shk = prefs.getInt("slic_shk", out.slic_shk);
    out.slic_line = prefs.getInt("slic_line", out.slic_line);
    out.slic_pd = prefs.getInt("slic_pd", out.slic_pd);
    out.hook_active_high = prefs.getBool("hook_hi", out.hook_active_high);
    prefs.end();

    String error;
    if (!validatePins(out, error)) {
        out = defaultPins();
        return false;
    }
    return true;
}

bool A252ConfigStore::savePins(const A252PinsConfig& cfg, String* error) {
    String local_error;
    if (!validatePins(cfg, local_error)) {
        if (error) {
            *error = local_error;
        }
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(kPinsNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }

    prefs.putInt("i2s_bck", cfg.i2s_bck);
    prefs.putInt("i2s_ws", cfg.i2s_ws);
    prefs.putInt("i2s_dout", cfg.i2s_dout);
    prefs.putInt("i2s_din", cfg.i2s_din);

    prefs.putInt("i2c_sda", cfg.es8388_sda);
    prefs.putInt("i2c_scl", cfg.es8388_scl);

    prefs.putInt("slic_rm", cfg.slic_rm);
    prefs.putInt("slic_fr", cfg.slic_fr);
    prefs.putInt("slic_shk", cfg.slic_shk);
    prefs.putInt("slic_line", cfg.slic_line);
    prefs.putInt("slic_pd", cfg.slic_pd);
    prefs.putBool("hook_hi", cfg.hook_active_high);
    prefs.end();
    return true;
}

bool A252ConfigStore::loadAudio(A252AudioConfig& out) {
    out = defaultAudio();
    Preferences prefs;
    if (!prefs.begin(kAudioNs, false)) {
        return false;
    }

    out.sample_rate = prefs.getUInt("sr", out.sample_rate);
    out.bits_per_sample = static_cast<uint8_t>(prefs.getUChar("bits", out.bits_per_sample));
    out.enable_capture = prefs.getBool("capture", out.enable_capture);
    out.volume = static_cast<uint8_t>(prefs.getUChar("vol", out.volume));
    out.mute = prefs.getBool("mute", out.mute);
    if (prefs.isKey("route")) {
        out.route = prefs.getString("route", out.route);
    }
    prefs.end();

    String error;
    if (!validateAudio(out, error)) {
        out = defaultAudio();
        return false;
    }
    return true;
}

bool A252ConfigStore::saveAudio(const A252AudioConfig& cfg, String* error) {
    String local_error;
    if (!validateAudio(cfg, local_error)) {
        if (error) {
            *error = local_error;
        }
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(kAudioNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }

    prefs.putUInt("sr", cfg.sample_rate);
    prefs.putUChar("bits", cfg.bits_per_sample);
    prefs.putBool("capture", cfg.enable_capture);
    prefs.putUChar("vol", cfg.volume);
    saveString(prefs, "route", cfg.route);
    prefs.putBool("mute", cfg.mute);
    prefs.end();
    return true;
}

bool A252ConfigStore::loadMqtt(MqttConfig& out) {
    out = defaultMqtt();
    Preferences prefs;
    if (!prefs.begin(kMqttNs, false)) {
        return false;
    }

    out.enabled = prefs.getBool("enabled", out.enabled);
    if (prefs.isKey("host")) {
        out.host = prefs.getString("host", out.host);
    }
    out.port = static_cast<uint16_t>(prefs.getUShort("port", out.port));
    if (prefs.isKey("user")) {
        out.user = prefs.getString("user", out.user);
    }
    if (prefs.isKey("pass")) {
        out.pass = prefs.getString("pass", out.pass);
    }
    if (prefs.isKey("topic")) {
        out.base_topic = prefs.getString("topic", out.base_topic);
    }
    prefs.end();

    String error;
    if (!validateMqtt(out, error)) {
        out = defaultMqtt();
        return false;
    }
    return true;
}

bool A252ConfigStore::saveMqtt(const MqttConfig& cfg, String* error) {
    String local_error;
    if (!validateMqtt(cfg, local_error)) {
        if (error) {
            *error = local_error;
        }
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(kMqttNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }

    prefs.putBool("enabled", cfg.enabled);
    saveString(prefs, "host", cfg.host);
    prefs.putUShort("port", cfg.port);
    saveString(prefs, "user", cfg.user);
    saveString(prefs, "pass", cfg.pass);
    saveString(prefs, "topic", cfg.base_topic);
    prefs.end();
    return true;
}

bool A252ConfigStore::loadEspNowPeers(EspNowPeerStore& out) {
    out.peers.clear();

    Preferences prefs;
    if (!prefs.begin(kEspNowNs, false)) {
        return false;
    }
    const String raw = prefs.isKey("peers") ? prefs.getString("peers", "[]") : String("[]");
    prefs.end();

    JsonDocument doc;
    if (!loadJsonArray(raw, doc)) {
        return false;
    }

    for (const JsonVariantConst item : doc.as<JsonArrayConst>()) {
        if (!item.is<const char*>()) {
            continue;
        }
        const String norm = normalizeMac(item.as<const char*>());
        if (norm.isEmpty()) {
            continue;
        }
        if (std::find(out.peers.begin(), out.peers.end(), norm) == out.peers.end()) {
            out.peers.push_back(norm);
        }
    }
    return true;
}

bool A252ConfigStore::saveEspNowPeers(const EspNowPeerStore& store, String* error) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const String& peer : store.peers) {
        if (!normalizeMac(peer).isEmpty()) {
            arr.add(normalizeMac(peer));
        }
    }

    String raw;
    serializeJson(arr, raw);

    Preferences prefs;
    if (!prefs.begin(kEspNowNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }
    saveString(prefs, "peers", raw);
    prefs.end();
    return true;
}

bool A252ConfigStore::validatePins(const A252PinsConfig& cfg, String& error) {
    std::vector<int> used;
    used.reserve(11);

    const int required_pins[] = {
        cfg.i2s_bck,
        cfg.i2s_ws,
        cfg.i2s_dout,
        cfg.i2s_din,
        cfg.es8388_sda,
        cfg.es8388_scl,
        cfg.slic_rm,
        cfg.slic_fr,
        cfg.slic_shk,
        cfg.slic_pd,
    };

    for (int pin : required_pins) {
        if (pin < 0 || pin > 39) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), pin) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(pin);
    }

    // Optional legacy line-enable pin, retired by default (-1).
    if (cfg.slic_line != -1) {
        if (cfg.slic_line < 0 || cfg.slic_line > 39) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), cfg.slic_line) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(cfg.slic_line);
    }

    error = "";
    return true;
}

bool A252ConfigStore::validateAudio(const A252AudioConfig& cfg, String& error) {
    if (cfg.sample_rate < 8000 || cfg.sample_rate > 48000) {
        error = "invalid_sample_rate";
        return false;
    }
    if (!(cfg.bits_per_sample == 16 || cfg.bits_per_sample == 24 || cfg.bits_per_sample == 32)) {
        error = "invalid_bits_per_sample";
        return false;
    }
    if (cfg.volume > 100) {
        error = "invalid_volume";
        return false;
    }

    const String route = cfg.route;
    if (!(route == "rtc" || route == "bluetooth" || route == "none")) {
        error = "invalid_route";
        return false;
    }

    error = "";
    return true;
}

bool A252ConfigStore::validateMqtt(const MqttConfig& cfg, String& error) {
    if (cfg.port == 0) {
        error = "invalid_port";
        return false;
    }

    if (cfg.base_topic.isEmpty()) {
        error = "invalid_base_topic";
        return false;
    }

    error = "";
    return true;
}

void A252ConfigStore::pinsToJson(const A252PinsConfig& cfg, JsonObject obj) {
    JsonObject i2s = obj["i2s"].to<JsonObject>();
    i2s["bck"] = cfg.i2s_bck;
    i2s["ws"] = cfg.i2s_ws;
    i2s["dout"] = cfg.i2s_dout;
    i2s["din"] = cfg.i2s_din;

    JsonObject i2c = obj["codec_i2c"].to<JsonObject>();
    i2c["sda"] = cfg.es8388_sda;
    i2c["scl"] = cfg.es8388_scl;

    JsonObject slic = obj["slic"].to<JsonObject>();
    slic["rm"] = cfg.slic_rm;
    slic["fr"] = cfg.slic_fr;
    slic["shk"] = cfg.slic_shk;
    slic["line"] = cfg.slic_line;
    slic["pd"] = cfg.slic_pd;
    slic["hook_active_high"] = cfg.hook_active_high;
}

void A252ConfigStore::audioToJson(const A252AudioConfig& cfg, JsonObject obj) {
    obj["sample_rate"] = cfg.sample_rate;
    obj["bits_per_sample"] = cfg.bits_per_sample;
    obj["enable_capture"] = cfg.enable_capture;
    obj["volume"] = cfg.volume;
    obj["mute"] = cfg.mute;
    obj["route"] = cfg.route;
}

void A252ConfigStore::mqttToJson(const MqttConfig& cfg, JsonObject obj, bool include_secret) {
    obj["enabled"] = cfg.enabled;
    obj["host"] = cfg.host;
    obj["port"] = cfg.port;
    obj["user"] = cfg.user;
    obj["base_topic"] = cfg.base_topic;
    if (include_secret) {
        obj["pass"] = cfg.pass;
    }
}

void A252ConfigStore::peersToJson(const EspNowPeerStore& store, JsonArray arr) {
    for (const String& peer : store.peers) {
        arr.add(peer);
    }
}

String A252ConfigStore::normalizeMac(const String& value) {
    String mac = value;
    mac.trim();
    mac.toUpperCase();

    String compact;
    compact.reserve(12);
    for (size_t i = 0; i < mac.length(); ++i) {
        const char c = mac[i];
        if (c == ':' || c == '-' || c == ' ') {
            continue;
        }
        const bool is_hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
        if (!is_hex) {
            return "";
        }
        compact += c;
    }

    if (compact.length() != 12) {
        return "";
    }

    String formatted;
    formatted.reserve(17);
    for (int i = 0; i < 12; i += 2) {
        if (i > 0) {
            formatted += ':';
        }
        formatted += compact.substring(i, i + 2);
    }
    return formatted;
}

bool A252ConfigStore::parseMac(const String& value, uint8_t out[6]) {
    const String formatted = normalizeMac(value);
    if (formatted.isEmpty()) {
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        const String chunk = formatted.substring(i * 3, i * 3 + 2);
        out[i] = static_cast<uint8_t>(strtoul(chunk.c_str(), nullptr, 16));
    }
    return true;
}
