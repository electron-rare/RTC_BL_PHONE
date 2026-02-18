#include "web/WebServerManager.h"

#include <Preferences.h>
#include <SPIFFS.h>

#include <algorithm>
#include <utility>

namespace {
constexpr size_t kMaxContacts = 200;
constexpr bool kForceAuthDisabled = true;
}

WebServerManager::WebServerManager(uint16_t port)
    : server_(port),
      rate_limit_ms_(1000),
      auth_enabled_(false),
      auth_user_("admin"),
      auth_pass_("admin"),
      config_param1_("valeur1"),
      config_param2_("valeur2") {}

void WebServerManager::begin() {
    loadAuthCredentials();

    if (contacts_.empty()) {
        contacts_ = {
            {"Alice", "+33612345678", "mobile"},
            {"Bob", "+33123456789", "fixe"},
        };
    }

    if (!SPIFFS.begin(true)) {
        Serial.println("[WebServerManager] SPIFFS mount failed");
        server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(503, "text/plain", "SPIFFS unavailable");
        });
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
    if (!isValidInput(user, 32) || !isValidInput(pass, 64)) {
        return;
    }
    auth_user_ = user;
    auth_pass_ = pass;
    if (persist_to_nvs) {
        persistAuthCredentials();
    }
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
    rate_limit_ms_ = std::max<uint32_t>(100, rate_limit_ms);
}

void WebServerManager::setControlCallback(
    std::function<bool(const String&, const JsonVariantConst&)> callback) {
    control_callback_ = std::move(callback);
}

void WebServerManager::setStatusCallback(std::function<void(JsonObject)> callback) {
    status_callback_ = std::move(callback);
}

void WebServerManager::registerRoutes() {
    server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(768);
        doc["state"] = "running";
        doc["battery"] = 90;
        doc["audio"] = "ok";
        doc["slic"] = "ok";
        doc["bluetooth"] = "ok";
        doc["wifi"] = "ok";
        doc["auth_enabled"] = isAuthEnabled();
        doc["auth_forced_disabled"] = kForceAuthDisabled;

        if (status_callback_) {
            JsonObject payload = doc.as<JsonObject>();
            status_callback_(payload);
        }

        request->send(200, "application/json", toJsonString(doc));
    });

    server_.on("/api/contacts", HTTP_GET, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(4096);
        JsonArray contacts = doc.to<JsonArray>();
        for (const auto& contact : contacts_) {
            JsonObject item = contacts.add<JsonObject>();
            item["nom"] = contact.nom;
            item["numero"] = contact.numero;
            item["type"] = contact.type;
        }
        request->send(200, "application/json", toJsonString(doc));
    });

    server_.on("/api/contacts", HTTP_POST, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(512);
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }

        const String nom = doc["nom"] | "";
        const String numero = doc["numero"] | "";
        const String type = doc["type"] | "mobile";
        if (!isValidInput(nom, 64) || !isValidInput(numero, 32) || !isValidInput(type, 16) ||
            contacts_.size() >= kMaxContacts) {
            request->send(400, "application/json", "{\"error\":\"invalid contact\"}");
            return;
        }
        contacts_.push_back({nom, numero, type});
        request->send(200, "application/json", "{\"result\":\"ajoute\"}");
    });

    server_.on("/api/contacts", HTTP_PUT, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(512);
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }

        const int index = doc["idx"] | -1;
        if (index < 0 || static_cast<size_t>(index) >= contacts_.size()) {
            request->send(400, "application/json", "{\"error\":\"invalid index\"}");
            return;
        }

        const String nom = doc["nom"] | "";
        const String numero = doc["numero"] | "";
        const String type = doc["type"] | "";
        if (!isValidInput(nom, 64) || !isValidInput(numero, 32) || !isValidInput(type, 16)) {
            request->send(400, "application/json", "{\"error\":\"invalid contact\"}");
            return;
        }
        contacts_[index] = {nom, numero, type};
        request->send(200, "application/json", "{\"result\":\"modifie\"}");
    });

    server_.on("/api/contacts", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(256);
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }
        const int index = doc["idx"] | -1;
        if (index < 0 || static_cast<size_t>(index) >= contacts_.size()) {
            request->send(400, "application/json", "{\"error\":\"invalid index\"}");
            return;
        }
        contacts_.erase(contacts_.begin() + index);
        request->send(200, "application/json", "{\"result\":\"supprime\"}");
    });

    server_.on("/api/contacts/sync_ble", HTTP_POST, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "{\"result\":\"sync BLE en preparation\"}");
    });

    server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        const String ip = request->client()->remoteIP().toString();
        if (isRateLimited(ip)) {
            request->send(429, "application/json", "{\"error\":\"rate limited\"}");
            return;
        }
        if (!authenticateRequest(request)) {
            return;
        }

        DynamicJsonDocument doc(384);
        doc["param1"] = config_param1_;
        doc["param2"] = config_param2_;
        doc["auth_enabled"] = isAuthEnabled();
        doc["auth_forced_disabled"] = kForceAuthDisabled;
        request->send(200, "application/json", toJsonString(doc));
    });

    server_.on("/api/config", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const String ip = request->client()->remoteIP().toString();
        if (isRateLimited(ip)) {
            request->send(429, "application/json", "{\"error\":\"rate limited\"}");
            return;
        }
        if (!authenticateRequest(request)) {
            return;
        }

        DynamicJsonDocument doc(512);
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }

        const String p1 = doc["param1"] | config_param1_;
        const String p2 = doc["param2"] | config_param2_;
        if (!isValidInput(p1, 64) || !isValidInput(p2, 64)) {
            request->send(400, "application/json", "{\"error\":\"invalid input\"}");
            return;
        }

        config_param1_ = p1;
        config_param2_ = p2;

        const String new_user = doc["auth_user"] | "";
        const String new_pass = doc["auth_pass"] | "";
        if (new_user.length() > 0 && new_pass.length() > 0) {
            setAuthCredentials(new_user, new_pass, true);
        }
        if (!kForceAuthDisabled && doc["auth_enabled"].is<bool>()) {
            setAuthEnabled(doc["auth_enabled"].as<bool>());
        }

        request->send(200, "application/json", "{\"result\":\"ok\"}");
    });

    server_.on("/api/logs", HTTP_GET, [this](AsyncWebServerRequest* request) {
        const String ip = request->client()->remoteIP().toString();
        if (isRateLimited(ip)) {
            request->send(429, "application/json", "{\"error\":\"rate limited\"}");
            return;
        }
        if (!authenticateRequest(request)) {
            return;
        }
        String logs = "Log systeme RTC_BL_PHONE\n";
        logs += "Batterie OK\nAudio OK\nSLIC OK\nBluetooth OK\nWiFi OK\n";
        request->send(200, "text/plain", logs);
    });

    server_.on("/api/control", HTTP_POST, [this](AsyncWebServerRequest* request) {
        const String ip = request->client()->remoteIP().toString();
        if (isRateLimited(ip)) {
            request->send(429, "application/json", "{\"error\":\"rate limited\"}");
            return;
        }
        if (!authenticateRequest(request)) {
            return;
        }

        DynamicJsonDocument doc(512);
        if (!extractJsonBody(request, doc)) {
            request->send(400, "application/json", "{\"error\":\"invalid json body\"}");
            return;
        }

        const String action = doc["action"] | "";
        if (!isValidInput(action, 32)) {
            request->send(400, "application/json", "{\"error\":\"invalid action\"}");
            return;
        }

        bool handled = false;
        if (control_callback_) {
            handled = control_callback_(action, doc.as<JsonVariantConst>());
        }
        if (!handled && action == "call") {
            handled = true;
        }

        if (handled) {
            request->send(200, "application/json", "{\"result\":\"action executee\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"unknown action\"}");
        }
    });
}

bool WebServerManager::isRateLimited(const String& ip) {
    const uint32_t now = millis();
    const auto it = last_request_ms_.find(ip);
    if (it != last_request_ms_.end() && now - it->second < rate_limit_ms_) {
        return true;
    }
    last_request_ms_[ip] = now;

    if (last_request_ms_.size() > 128) {
        for (auto iter = last_request_ms_.begin(); iter != last_request_ms_.end();) {
            if (now - iter->second > 60000) {
                iter = last_request_ms_.erase(iter);
            } else {
                ++iter;
            }
        }
    }
    return false;
}

bool WebServerManager::authenticateRequest(AsyncWebServerRequest* request) {
    if (!isAuthEnabled()) {
        return true;
    }
    if (!request->authenticate(auth_user_.c_str(), auth_pass_.c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

bool WebServerManager::extractJsonBody(AsyncWebServerRequest* request, DynamicJsonDocument& doc) {
    if (!request->hasParam("plain", true)) {
        return false;
    }
    const String body = request->getParam("plain", true)->value();
    return deserializeJson(doc, body) == DeserializationError::Ok;
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
        if (c == '<' || c == '>' || c == '\\' || c == '`') {
            return false;
        }
    }
    return true;
}

String WebServerManager::toJsonString(const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    return out;
}

void WebServerManager::loadAuthCredentials() {
    auth_enabled_ = false;
    if (kForceAuthDisabled) {
        return;
    }

    Preferences prefs;
    if (!prefs.begin("web-auth", true)) {
        return;
    }
    auth_user_ = prefs.getString("user", auth_user_);
    auth_pass_ = prefs.getString("pass", auth_pass_);
    auth_enabled_ = prefs.getBool("enabled", auth_enabled_);
    prefs.end();
}

void WebServerManager::persistAuthCredentials() const {
    if (kForceAuthDisabled) {
        return;
    }

    Preferences prefs;
    if (!prefs.begin("web-auth", false)) {
        return;
    }
    prefs.putString("user", auth_user_);
    prefs.putString("pass", auth_pass_);
    prefs.putBool("enabled", auth_enabled_);
    prefs.end();
}
