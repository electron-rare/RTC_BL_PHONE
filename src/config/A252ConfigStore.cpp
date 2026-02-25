#include "config/A252ConfigStore.h"

#include <Preferences.h>

#include <algorithm>

#include "core/PlatformProfile.h"

namespace {
constexpr const char* kPinsNs = "a252-pins";
constexpr const char* kAudioNs = "a252-audio";
constexpr const char* kMqttNs = "mqtt";
constexpr const char* kEspNowNs = "espnow";
constexpr const char* kEspNowCallMapNs = "espnow-call";

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

bool loadJsonObject(const String& raw, JsonDocument& doc) {
    if (raw.isEmpty()) {
        doc.to<JsonObject>();
        return true;
    }
    const auto err = deserializeJson(doc, raw);
    return err == DeserializationError::Ok && doc.is<JsonObject>();
}

String normalizeEspNowCallKeyword(const String& keyword) {
    String normalized = keyword;
    normalized.trim();
    normalized.toUpperCase();
    return normalized;
}

void mergeCallMapEntry(EspNowCallMap& map, const String& keyword, const String& path) {
    const String normalized_keyword = normalizeEspNowCallKeyword(keyword);
    if (normalized_keyword.isEmpty()) {
        return;
    }
    if (path.isEmpty()) {
        return;
    }

    for (EspNowCallMapEntry& entry : map) {
        if (entry.keyword == normalized_keyword) {
            entry.path = path;
            return;
        }
    }
    map.push_back({normalized_keyword, path});
}

}  // namespace

A252PinsConfig A252ConfigStore::defaultPins() {
    A252PinsConfig cfg;
    if (detectBoardProfile() == BoardProfile::ESP32_S3) {
        cfg.i2s_bck = 17;
        cfg.i2s_ws = 18;
        cfg.i2s_dout = 21;
        cfg.i2s_din = 39;
        cfg.es8388_sda = -1;
        cfg.es8388_scl = -1;
        cfg.slic_rm = 32;
        cfg.slic_fr = 5;
        cfg.slic_shk = 23;
        cfg.slic_pd = 14;
        cfg.slic_adc_in = 34;
        cfg.hook_active_high = true;
        cfg.pcm_flt = 25;
        cfg.pcm_demp = 26;
        cfg.pcm_xsmt = 27;
        cfg.pcm_fmt = 33;
    }
    return cfg;
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
    out.slic_adc_in = prefs.getInt("slic_adc_in", out.slic_adc_in);
    out.hook_active_high = prefs.getBool("hook_hi", out.hook_active_high);
    out.pcm_flt = prefs.getInt("pcm_flt", out.pcm_flt);
    out.pcm_demp = prefs.getInt("pcm_demp", out.pcm_demp);
    out.pcm_xsmt = prefs.getInt("pcm_xsmt", out.pcm_xsmt);
    out.pcm_fmt = prefs.getInt("pcm_fmt", out.pcm_fmt);
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
    prefs.putInt("slic_adc_in", cfg.slic_adc_in);
    prefs.putBool("hook_hi", cfg.hook_active_high);
    prefs.putInt("pcm_flt", cfg.pcm_flt);
    prefs.putInt("pcm_demp", cfg.pcm_demp);
    prefs.putInt("pcm_xsmt", cfg.pcm_xsmt);
    prefs.putInt("pcm_fmt", cfg.pcm_fmt);
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
    out.adc_dsp_enabled = prefs.getBool("adc_dsp", out.adc_dsp_enabled);
    out.adc_fft_enabled = prefs.getBool("adc_fft", out.adc_fft_enabled);
    out.adc_dsp_fft_downsample = static_cast<uint8_t>(prefs.getUChar("adc_dsp_fft_downsample", out.adc_dsp_fft_downsample));
    out.adc_fft_ignore_low_bin =
        static_cast<uint16_t>(prefs.getUInt("adc_fft_ignore_low_bin", out.adc_fft_ignore_low_bin));
    out.adc_fft_ignore_high_bin =
        static_cast<uint16_t>(prefs.getUInt("adc_fft_ignore_high_bin", out.adc_fft_ignore_high_bin));
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
    prefs.putBool("adc_dsp", cfg.adc_dsp_enabled);
    prefs.putBool("adc_fft", cfg.adc_fft_enabled);
    prefs.putUChar("adc_dsp_fft_downsample", cfg.adc_dsp_fft_downsample);
    prefs.putUInt("adc_fft_ignore_low_bin", cfg.adc_fft_ignore_low_bin);
    prefs.putUInt("adc_fft_ignore_high_bin", cfg.adc_fft_ignore_high_bin);
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

bool A252ConfigStore::loadEspNowCallMap(EspNowCallMap& out) {
    out.clear();
    Preferences prefs;
    if (!prefs.begin(kEspNowCallMapNs, false)) {
        return false;
    }
    const String raw = prefs.isKey("mappings") ? prefs.getString("mappings", "{}") : String("{}");
    prefs.end();

    JsonDocument doc;
    if (!loadJsonObject(raw, doc)) {
        return false;
    }

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair item : obj) {
        if (!item.value().is<const char*>()) {
            continue;
        }
        const String key = item.key().c_str();
        mergeCallMapEntry(out, key, item.value().as<const char*>());
    }
    return true;
}

bool A252ConfigStore::saveEspNowCallMap(const EspNowCallMap& map, String* error) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    for (const EspNowCallMapEntry& entry : map) {
        if (entry.keyword.isEmpty() || entry.path.isEmpty()) {
            continue;
        }
        obj[entry.keyword] = entry.path;
    }

    String raw;
    serializeJson(obj, raw);

    Preferences prefs;
    if (!prefs.begin(kEspNowCallMapNs, false)) {
        if (error) {
            *error = "nvs_open_failed";
        }
        return false;
    }
    const bool ok = prefs.putString("mappings", raw) >= 0;
    prefs.end();
    return ok;
}

void A252ConfigStore::espNowCallMapToJson(const EspNowCallMap& map, JsonObject obj) {
    for (const EspNowCallMapEntry& entry : map) {
        if (entry.keyword.isEmpty() || entry.path.isEmpty()) {
            continue;
        }
        obj[entry.keyword] = entry.path;
    }
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
        cfg.slic_rm,
        cfg.slic_fr,
        cfg.slic_shk,
        cfg.slic_pd,
        cfg.pcm_flt,
        cfg.pcm_demp,
        cfg.pcm_xsmt,
        cfg.pcm_fmt,
        cfg.slic_adc_in,
    };

    for (int pin : required_pins) {
        if (pin < 0) {
            continue;
        }
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

    if (detectBoardProfile() == BoardProfile::ESP32_A252) {
        if (cfg.es8388_sda < 0 || cfg.es8388_scl < 0) {
            error = "invalid_pin_range";
            return false;
        }
        if (cfg.es8388_sda == cfg.es8388_scl) {
            error = "pin_conflict";
            return false;
        }
        if (cfg.es8388_sda < 0 || cfg.es8388_sda > 39 || cfg.es8388_scl < 0 || cfg.es8388_scl > 39) {
            error = "invalid_pin_range";
            return false;
        }
        if (std::find(used.begin(), used.end(), cfg.es8388_sda) != used.end() ||
            std::find(used.begin(), used.end(), cfg.es8388_scl) != used.end()) {
            error = "pin_conflict";
            return false;
        }
        used.push_back(cfg.es8388_sda);
        used.push_back(cfg.es8388_scl);
    } else {
        if (cfg.es8388_sda >= 0) {
            if (cfg.es8388_sda > 39 || std::find(used.begin(), used.end(), cfg.es8388_sda) != used.end()) {
                error = cfg.es8388_sda > 39 ? "invalid_pin_range" : "pin_conflict";
                return false;
            }
            used.push_back(cfg.es8388_sda);
        }
        if (cfg.es8388_scl >= 0) {
            if (cfg.es8388_scl > 39 || std::find(used.begin(), used.end(), cfg.es8388_scl) != used.end()) {
                error = cfg.es8388_scl > 39 ? "invalid_pin_range" : "pin_conflict";
                return false;
            }
            used.push_back(cfg.es8388_scl);
        }
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
    if (cfg.adc_dsp_fft_downsample == 0U || cfg.adc_dsp_fft_downsample > 64U) {
        error = "invalid_adc_dsp_fft_downsample";
        return false;
    }
    if (cfg.adc_fft_ignore_low_bin > 32U) {
        error = "invalid_adc_fft_ignore_low_bin";
        return false;
    }
    if (cfg.adc_fft_ignore_high_bin > 32U) {
        error = "invalid_adc_fft_ignore_high_bin";
        return false;
    }
    if (cfg.volume > 100) {
        error = "invalid_volume";
        return false;
    }

    const String route = cfg.route;
    if (!(route == "rtc" || route == "none")) {
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
    slic["adc_in"] = cfg.slic_adc_in;
    slic["hook_active_high"] = cfg.hook_active_high;

    JsonObject pcm = obj["pcm"].to<JsonObject>();
    pcm["flt"] = cfg.pcm_flt;
    pcm["demp"] = cfg.pcm_demp;
    pcm["xsmt"] = cfg.pcm_xsmt;
    pcm["fmt"] = cfg.pcm_fmt;
}

void A252ConfigStore::audioToJson(const A252AudioConfig& cfg, JsonObject obj) {
    obj["sample_rate"] = cfg.sample_rate;
    obj["bits_per_sample"] = cfg.bits_per_sample;
    obj["enable_capture"] = cfg.enable_capture;
    obj["adc_dsp_enabled"] = cfg.adc_dsp_enabled;
    obj["adc_fft_enabled"] = cfg.adc_fft_enabled;
    obj["adc_dsp_fft_downsample"] = cfg.adc_dsp_fft_downsample;
    obj["adc_fft_ignore_low_bin"] = cfg.adc_fft_ignore_low_bin;
    obj["adc_fft_ignore_high_bin"] = cfg.adc_fft_ignore_high_bin;
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
