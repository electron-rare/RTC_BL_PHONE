#include "core/AgentSupervisor.h"
#include <Arduino.h>

void notifyWifi(const std::string& state, const std::string& error = "") {
    AgentStatus status{state, error, millis()};
    AgentSupervisor::instance().notify("wifi", status);
}
#include "wifi/WifiManager.h"

WifiManager::WifiManager() : connected_(false), ssid_("") {}

bool WifiManager::begin(const char* ssid, const char* password, uint32_t timeout_ms) {
    if (ssid == nullptr || ssid[0] == '\0') {
        connected_ = false;
        notifyWifi("init_failed", "no ssid");
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
    notifyWifi(connected_ ? "connected" : "connect_failed");
    return connected_;
}

void WifiManager::loop() {
    connected_ = WiFi.status() == WL_CONNECTED;
    notifyWifi(connected_ ? "connected" : "disconnected");
}

bool WifiManager::isConnected() const {
    return connected_;
}
