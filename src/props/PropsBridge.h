#ifndef PROPS_PROPS_BRIDGE_H
#define PROPS_PROPS_BRIDGE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include <functional>

#include "config/A252ConfigStore.h"

class PropsBridge {
public:
    PropsBridge();

    bool begin(const MqttConfig& config);
    void setConfig(const MqttConfig& config);
    const MqttConfig& config() const;

    bool connectNow();
    void disconnect();
    bool isConnected() const;
    void tick();

    bool publish(const String& topic_suffix, const String& payload, bool retained = false);
    bool publishRaw(const String& topic, const String& payload, bool retained = false);
    void publishStatus(const JsonVariantConst& status);
    void publishEvent(const JsonVariantConst& event);
    void handleCommand(const JsonVariantConst& cmd);

    void setCommandCallback(std::function<void(const String&, const JsonVariantConst&)> cb);
    void statusToJson(JsonObject obj) const;

private:
    void ensureClientConfigured();
    bool connectInternal();
    String topicFor(const String& suffix) const;

    WiFiClient net_client_;
    mutable PubSubClient mqtt_client_;
    MqttConfig config_;
    std::function<void(const String&, const JsonVariantConst&)> command_callback_;

    bool initialized_ = false;
    uint32_t next_reconnect_ms_ = 0;
    uint32_t reconnect_backoff_ms_ = 3000;
    uint32_t reconnect_attempts_ = 0;
};

#endif  // PROPS_PROPS_BRIDGE_H
