#include "wifi/WifiManager.h"

WifiManager::WifiManager() : connected_(false), ssid_("") {}

bool WifiManager::begin(const char* ssid, const char* password, uint32_t timeout_ms) {
    if (ssid == nullptr || ssid[0] == '\0') {
        connected_ = false;
        return false;
    }

    ssid_ = ssid;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    const uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < timeout_ms) {
        delay(100);
    }
    connected_ = WiFi.status() == WL_CONNECTED;
    return connected_;
}

void WifiManager::loop() {
    connected_ = WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isConnected() const {
    return connected_;
}
