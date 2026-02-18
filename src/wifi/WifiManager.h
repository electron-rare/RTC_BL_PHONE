// WifiManager.h
// Gestion WiFi (connexion, OTA, logs)

#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <WiFi.h>

class WifiManager {
public:
    WifiManager();
    bool begin(const char* ssid, const char* password, uint32_t timeout_ms = 10000);
    void loop();
    bool isConnected() const;

private:
    bool connected_;
    String ssid_;
};

#endif // WIFIMANAGER_H
