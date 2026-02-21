#include "web/WebServerManager.h"

#include <SPIFFS.h>

namespace {
constexpr bool kForceAuthDisabled = true;

String quoteArg(const String& value) {
    String escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("\"", "\\\"");
    return String("\"") + escaped + "\"";
}
}

WebServerManager::WebServerManager(uint16_t port)
    : server_(port), rate_limit_ms_(250), auth_enabled_(false), auth_user_("admin"), auth_pass_("admin") {}

void WebServerManager::begin() {
    if (!SPIFFS.begin(true)) {
        Serial.println("[WebServerManager] SPIFFS mount failed");
    } else {
        server_.serveStatic("/", SPIFFS, "/webui/").setDefaultFile("index.html");
    }

    registerRoutes();
    server_.begin();
    Serial.println("[WebServerManager] HTTP server started");
}

void WebServerManager::handle() {
    // ESPAsyncWebServer is event-driven.
}

void WebServerManager::setAuthCredentials(const String& user, const String& pass, bool persist_to_nvs) {
    (void)persist_to_nvs;
    if (!isValidInput(user, 32) || !isValidInput(pass, 64)) {
        return;
    }
    auth_user_ = user;
    auth_pass_ = pass;
}

void WebServerManager::setAuthEnabled(bool enabled) {
    if (kForceAuthDisabled) {
        auth_enabled_ = false;
        return;
    }
    auth_enabled_ = enabled;
}

bool WebServerManager::isAuthEnabled() const {
    return auth_enabled_;
}

void WebServerManager::setRateLimitMs(uint32_t rate_limit_ms) {
    rate_limit_ms_ = rate_limit_ms;
}

void WebServerManager::setStatusCallback(std::function<void(JsonObject)> callback) {
    status_callback_ = std::move(callback);
}

void WebServerManager::setCommandExecutor(std::function<DispatchResponse(const String&)> callback) {
    command_executor_ = std::move(callback);
}

void WebServerManager::registerRoutes() {
    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["auth_enabled"] = isAuthEnabled();
        if (status_callback_) {
            status_callback_(doc.to<JsonObject>());
        }
        request->send(200, "application/json", toJsonString(doc));
    });

    server_.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String action = doc["action"] | "";
        if (!isValidInput(action, 128)) {
            request->send(400, "application/json", "{\"error\":\"invalid action\"}");
            return;
        }
        handleDispatch(request, action);
    });

    // A252 config endpoints.
    server_.on("/api/config/pins", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "SLIC_CONFIG_GET"); });
    server_.on("/api/config/pins", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        String payload;
        serializeJson(doc, payload);
        handleDispatch(request, "SLIC_CONFIG_SET " + payload);
    });

    server_.on("/api/config/audio", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "AUDIO_CONFIG_GET"); });
    server_.on("/api/config/audio", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        String payload;
        serializeJson(doc, payload);
        handleDispatch(request, "AUDIO_CONFIG_SET " + payload);
    });

    server_.on("/api/config/mqtt", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "MQTT_STATUS"); });
    server_.on("/api/config/mqtt", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        String payload;
        serializeJson(doc, payload);
        handleDispatch(request, "MQTT_CONFIG_SET " + payload);
    });

    // WiFi.
    server_.on("/api/network/wifi", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_STATUS"); });
    server_.on("/api/network/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String ssid = doc["ssid"] | "";
        const String pass = doc["pass"] | "";
        if (!isValidInput(ssid, 64)) {
            request->send(400, "application/json", "{\"error\":\"invalid ssid\"}");
            return;
        }
        handleDispatch(request, "WIFI_CONNECT " + quoteArg(ssid) + " " + quoteArg(pass));
    });
    server_.on("/api/network/wifi/disconnect", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_DISCONNECT"); });
    server_.on("/api/network/wifi/scan", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "WIFI_SCAN"); });

    // MQTT.
    server_.on("/api/network/mqtt", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "MQTT_STATUS"); });
    server_.on("/api/network/mqtt/connect", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "MQTT_CONNECT"); });
    server_.on("/api/network/mqtt/disconnect", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "MQTT_DISCONNECT"); });
    server_.on("/api/network/mqtt/publish", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String topic = doc["topic"] | "";
        const String payload = doc["payload"] | "";
        if (!isValidInput(topic, 128)) {
            request->send(400, "application/json", "{\"error\":\"invalid topic\"}");
            return;
        }
        handleDispatch(request, "MQTT_PUB " + topic + " " + payload);
    });

    // ESP-NOW.
    server_.on("/api/network/espnow", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "ESPNOW_STATUS"); });
    server_.on("/api/network/espnow/peer", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "ESPNOW_PEER_LIST"); });
    server_.on("/api/network/espnow/peer", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String mac = doc["mac"] | "";
        if (!isValidInput(mac, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid mac\"}");
            return;
        }
        handleDispatch(request, "ESPNOW_PEER_ADD " + mac);
    });
    server_.on("/api/network/espnow/peer", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String mac = doc["mac"] | "";
        if (!isValidInput(mac, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid mac\"}");
            return;
        }
        handleDispatch(request, "ESPNOW_PEER_DEL " + mac);
    });
    server_.on("/api/network/espnow/send", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String mac = doc["mac"] | "broadcast";
        String payload;
        if (doc["payload"].is<JsonVariant>()) {
            serializeJson(doc["payload"], payload);
        } else {
            payload = doc["payload"] | "{}";
        }
        handleDispatch(request, "ESPNOW_SEND " + mac + " " + payload);
    });

    // Bluetooth.
    server_.on("/api/bluetooth", HTTP_GET,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "BT_STATUS"); });
    server_.on("/api/bluetooth/hfp/connect", HTTP_POST, [this](AsyncWebServerRequest* request) {
        JsonDocument doc;
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const String addr = doc["addr"] | "";
        if (!isValidInput(addr, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid addr\"}");
            return;
        }
        handleDispatch(request, "BT_HFP_CONNECT " + addr);
    });
    server_.on("/api/bluetooth/hfp/disconnect", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "BT_HFP_DISCONNECT"); });
    server_.on("/api/bluetooth/ble/start", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "BT_BLE_START"); });
    server_.on("/api/bluetooth/ble/stop", HTTP_POST,
               [this](AsyncWebServerRequest* request) { handleDispatch(request, "BT_BLE_STOP"); });
}

bool WebServerManager::authenticateRequest(AsyncWebServerRequest* request) const {
    if (kForceAuthDisabled || !auth_enabled_) {
        return true;
    }
    if (!request->authenticate(auth_user_.c_str(), auth_pass_.c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

bool WebServerManager::extractJsonBody(AsyncWebServerRequest* request, JsonDocument& doc) {
    if (request->hasParam("plain", true)) {
        const String body = request->getParam("plain", true)->value();
        return deserializeJson(doc, body) == DeserializationError::Ok;
    }
    return false;
}

String WebServerManager::toJsonString(const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    return out;
}

bool WebServerManager::isValidInput(const String& value, size_t max_len) {
    if (value.isEmpty() || value.length() > max_len) {
        return false;
    }
    for (size_t i = 0; i < value.length(); ++i) {
        const char c = value[i];
        if (c < 32 || c > 126) {
            return false;
        }
    }
    return true;
}

void WebServerManager::handleDispatch(AsyncWebServerRequest* request,
                                      const String& command_line,
                                      uint16_t success_code,
                                      uint16_t error_code) {
    if (!authenticateRequest(request)) {
        return;
    }
    if (!command_executor_) {
        request->send(500, "application/json", "{\"error\":\"command executor not configured\"}");
        return;
    }

    const DispatchResponse res = command_executor_(command_line);

    if (!res.json.isEmpty()) {
        request->send(res.ok ? success_code : error_code, "application/json", res.json);
        return;
    }

    JsonDocument doc;
    doc["ok"] = res.ok;
    if (!res.code.isEmpty()) {
        doc["code"] = res.code;
    }
    if (!res.raw.isEmpty()) {
        doc["raw"] = res.raw;
    }
    request->send(res.ok ? success_code : error_code, "application/json", toJsonString(doc));
}
