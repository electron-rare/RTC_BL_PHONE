#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <WiFi.h>
#include <ArduinoJson.h>

struct WifiStatusSnapshot {
    bool connected = false;
    bool has_credentials = false;
    String ssid;
    String ip;
    int32_t rssi = 0;
    String state;
};

class WifiManager {
public:
    WifiManager();

    bool begin(const char* ssid, const char* password, uint32_t timeout_ms = 10000);
    bool connect(const String& ssid, const String& password, uint32_t timeout_ms = 10000,
                 bool persist = true);
    bool reconnect(uint32_t timeout_ms = 10000);
    void disconnect(bool erase_credentials = false);
    void loop();

    bool isConnected() const;
    bool hasCredentials() const;
    WifiStatusSnapshot status() const;
    void statusToJson(JsonObject obj) const;
    void scanToJson(JsonArray arr, int max_networks = 20) const;

private:
    bool connected_;
    String ssid_;
    String password_;
    mutable uint32_t next_auto_reconnect_ms_;
    uint32_t reconnect_backoff_ms_;

    bool waitForConnection(uint32_t timeout_ms);
};

#endif  // WIFIMANAGER_H
