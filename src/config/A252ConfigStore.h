#ifndef CONFIG_A252_CONFIG_STORE_H
#define CONFIG_A252_CONFIG_STORE_H

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

struct A252PinsConfig {
    int i2s_bck = 27;
    int i2s_ws = 25;
    int i2s_dout = 26;
    int i2s_din = 35;

    int es8388_sda = 33;
    int es8388_scl = 32;

    int slic_rm = 22;
    int slic_fr = 19;
    int slic_shk = 36;
    int slic_line = -1;
    int slic_pd = 18;
    bool hook_active_high = false;
};

struct A252AudioConfig {
    uint32_t sample_rate = 16000;
    uint8_t bits_per_sample = 16;
    bool enable_capture = true;
    uint8_t volume = 90;
    bool mute = false;
    String route = "rtc";
};

struct MqttConfig {
    bool enabled = false;
    String host;
    uint16_t port = 1883;
    String user;
    String pass;
    String base_topic = "rtc_bl_phone/a252";
};

struct EspNowPeerStore {
    std::vector<String> peers;
};

class A252ConfigStore {
public:
    static A252PinsConfig defaultPins();
    static A252AudioConfig defaultAudio();
    static MqttConfig defaultMqtt();

    static bool loadPins(A252PinsConfig& out);
    static bool savePins(const A252PinsConfig& cfg, String* error = nullptr);

    static bool loadAudio(A252AudioConfig& out);
    static bool saveAudio(const A252AudioConfig& cfg, String* error = nullptr);

    static bool loadMqtt(MqttConfig& out);
    static bool saveMqtt(const MqttConfig& cfg, String* error = nullptr);

    static bool loadEspNowPeers(EspNowPeerStore& out);
    static bool saveEspNowPeers(const EspNowPeerStore& store, String* error = nullptr);

    static bool validatePins(const A252PinsConfig& cfg, String& error);
    static bool validateAudio(const A252AudioConfig& cfg, String& error);
    static bool validateMqtt(const MqttConfig& cfg, String& error);

    static void pinsToJson(const A252PinsConfig& cfg, JsonObject obj);
    static void audioToJson(const A252AudioConfig& cfg, JsonObject obj);
    static void mqttToJson(const MqttConfig& cfg, JsonObject obj, bool include_secret = false);
    static void peersToJson(const EspNowPeerStore& store, JsonArray arr);

    static String normalizeMac(const String& value);
    static bool parseMac(const String& value, uint8_t out[6]);
};

#endif  // CONFIG_A252_CONFIG_STORE_H
