#include "props/PropsBridge.h"

#include <WiFi.h>

namespace {
String clientId() {
    return String("rtc-bl-") + String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF), HEX);
}

}  // namespace

PropsBridge::PropsBridge() : mqtt_client_(net_client_) {}

bool PropsBridge::begin(const MqttConfig& config) {
    config_ = config;
    ensureClientConfigured();
    initialized_ = true;
    reconnect_attempts_ = 0;
    next_reconnect_ms_ = 0;
    return true;
}

void PropsBridge::setConfig(const MqttConfig& config) {
    config_ = config;
    ensureClientConfigured();
    disconnect();
}

const MqttConfig& PropsBridge::config() const {
    return config_;
}

bool PropsBridge::connectNow() {
    if (!initialized_) {
        return false;
    }
    return connectInternal();
}

void PropsBridge::disconnect() {
    if (mqtt_client_.connected()) {
        mqtt_client_.disconnect();
    }
}

bool PropsBridge::isConnected() const {
    return mqtt_client_.connected();
}

void PropsBridge::tick() {
    if (!initialized_ || !config_.enabled) {
        return;
    }

    if (mqtt_client_.connected()) {
        mqtt_client_.loop();
        return;
    }

    if (!WiFi.isConnected()) {
        return;
    }

    if (next_reconnect_ms_ != 0 && millis() < next_reconnect_ms_) {
        return;
    }

    if (!connectInternal()) {
        reconnect_attempts_++;
        const uint32_t exp = (reconnect_attempts_ < 5) ? reconnect_attempts_ : 5;
        next_reconnect_ms_ = millis() + reconnect_backoff_ms_ * (1U << exp);
    }
}

bool PropsBridge::publish(const String& topic_suffix, const String& payload, bool retained) {
    return publishRaw(topicFor(topic_suffix), payload, retained);
}

bool PropsBridge::publishRaw(const String& topic, const String& payload, bool retained) {
    if (!mqtt_client_.connected()) {
        return false;
    }
    return mqtt_client_.publish(topic.c_str(), payload.c_str(), retained);
}

void PropsBridge::publishStatus(const JsonVariantConst& status) {
    String payload;
    serializeJson(status, payload);
    publish("status", payload, false);
}

void PropsBridge::publishEvent(const JsonVariantConst& event) {
    String payload;
    serializeJson(event, payload);
    publish("event", payload, false);
}

void PropsBridge::handleCommand(const JsonVariantConst& cmd) {
    if (!command_callback_) {
        return;
    }

    String source = "direct";
    if (cmd["source"].is<const char*>()) {
        source = cmd["source"].as<const char*>();
    }
    command_callback_(source, cmd);
}

void PropsBridge::setCommandCallback(std::function<void(const String&, const JsonVariantConst&)> cb) {
    command_callback_ = std::move(cb);
}

void PropsBridge::statusToJson(JsonObject obj) const {
    obj["enabled"] = config_.enabled;
    obj["connected"] = mqtt_client_.connected();
    obj["host"] = config_.host;
    obj["port"] = config_.port;
    obj["base_topic"] = config_.base_topic;
    obj["reconnect_attempts"] = reconnect_attempts_;
}

void PropsBridge::ensureClientConfigured() {
    mqtt_client_.setServer(config_.host.c_str(), config_.port);
    mqtt_client_.setBufferSize(768);
    mqtt_client_.setKeepAlive(30);

    mqtt_client_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
        JsonDocument doc;
        String body;
        body.reserve(length + 1);
        for (unsigned int i = 0; i < length; ++i) {
            body += static_cast<char>(payload[i]);
        }

        String action;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            action = doc["cmd"] | "";
        } else {
            action = body;
            doc.clear();
            doc["raw"] = body;
        }

        if (!command_callback_) {
            return;
        }

        String topic_str = topic;
        command_callback_(topic_str, doc.as<JsonVariantConst>());
        if (!action.isEmpty()) {
            JsonDocument evt;
            evt["cmd"] = action;
            evt["source"] = "mqtt";
        }
    });
}

bool PropsBridge::connectInternal() {
    if (!config_.enabled || config_.host.isEmpty() || !WiFi.isConnected()) {
        return false;
    }

    ensureClientConfigured();

    const String cid = clientId();
    const bool ok = config_.user.isEmpty()
                        ? mqtt_client_.connect(cid.c_str())
                        : mqtt_client_.connect(cid.c_str(), config_.user.c_str(), config_.pass.c_str());
    if (!ok) {
        return false;
    }

    reconnect_attempts_ = 0;
    next_reconnect_ms_ = 0;
    mqtt_client_.subscribe(topicFor("cmd").c_str());
    return true;
}

String PropsBridge::topicFor(const String& suffix) const {
    String topic = config_.base_topic;
    if (!topic.endsWith("/")) {
        topic += '/';
    }
    topic += suffix;
    return topic;
}
