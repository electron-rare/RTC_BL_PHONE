#ifndef WEB_WEB_SERVER_MANAGER_H
#define WEB_WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include <functional>
#include <map>
#include <vector>

struct WebContact {
    String nom;
    String numero;
    String type;
};

class WebServerManager {
public:
    explicit WebServerManager(uint16_t port = 80);
    void begin();
    void handle();

    void setAuthCredentials(const String& user, const String& pass, bool persist_to_nvs = false);
    void setAuthEnabled(bool enabled);
    bool isAuthEnabled() const;
    void setRateLimitMs(uint32_t rate_limit_ms);

    void setControlCallback(std::function<bool(const String&, const JsonVariantConst&)> callback);
    void setStatusCallback(std::function<void(JsonObject)> callback);

private:
    AsyncWebServer server_;
    uint32_t rate_limit_ms_;
    bool auth_enabled_;
    String auth_user_;
    String auth_pass_;
    String config_param1_;
    String config_param2_;
    std::vector<WebContact> contacts_;
    std::map<String, uint32_t> last_request_ms_;
    std::function<bool(const String&, const JsonVariantConst&)> control_callback_;
    std::function<void(JsonObject)> status_callback_;

    void registerRoutes();
    bool isRateLimited(const String& ip);
    bool authenticateRequest(AsyncWebServerRequest* request);
    static bool extractJsonBody(AsyncWebServerRequest* request, DynamicJsonDocument& doc);
    static bool isValidInput(const String& value, size_t max_len);
    static String toJsonString(const JsonDocument& doc);
    void loadAuthCredentials();
    void persistAuthCredentials() const;
};

#endif  // WEB_WEB_SERVER_MANAGER_H
