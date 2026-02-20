#include "wifi/WifiManager.h"

#include "core/AgentSupervisor.h"
#include "wifi/WifiCredentialsStorage.h"

#include <Arduino.h>

namespace {
void notifyWifi(const std::string& state, const std::string& error = "") {
    AgentStatus status{state, error, millis()};
    AgentSupervisor::instance().notify("wifi", status);
}

String wifiStateToString(wl_status_t status) {
    switch (status) {
        case WL_CONNECTED:
            return "connected";
        case WL_IDLE_STATUS:
            return "idle";
        case WL_NO_SSID_AVAIL:
            return "no_ssid";
        case WL_SCAN_COMPLETED:
            return "scan_completed";
        case WL_CONNECT_FAILED:
            return "connect_failed";
        case WL_CONNECTION_LOST:
            return "connection_lost";
        case WL_DISCONNECTED:
            return "disconnected";
        default:
            return "unknown";
    }
}

}  // namespace

WifiManager::WifiManager()
    : connected_(false), ssid_(""), password_(""), next_auto_reconnect_ms_(0), reconnect_backoff_ms_(3000) {}

bool WifiManager::begin(const char* ssid, const char* password, uint32_t timeout_ms) {
    return connect(ssid ? String(ssid) : "", password ? String(password) : "", timeout_ms, true);
}

bool WifiManager::connect(const String& ssid, const String& password, uint32_t timeout_ms, bool persist) {
    if (ssid.isEmpty()) {
        connected_ = false;
        notifyWifi("init_failed", "no_ssid");
        return false;
    }

    ssid_ = ssid;
    password_ = password;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    delay(100);
    WiFi.begin(ssid_.c_str(), password_.c_str());

    connected_ = waitForConnection(timeout_ms);
    if (connected_) {
        if (persist) {
            WifiCredentialsStorage::save(ssid_, password_);
        }
        notifyWifi("connected");
        next_auto_reconnect_ms_ = 0;
    } else {
        notifyWifi("connect_failed");
        next_auto_reconnect_ms_ = millis() + reconnect_backoff_ms_;
    }
    return connected_;
}

bool WifiManager::reconnect(uint32_t timeout_ms) {
    if (ssid_.isEmpty()) {
        String ssid;
        String password;
        if (!WifiCredentialsStorage::load(ssid, password)) {
            notifyWifi("reconnect_failed", "no_credentials");
            return false;
        }
        ssid_ = ssid;
        password_ = password;
    }
    return connect(ssid_, password_, timeout_ms, false);
}

void WifiManager::disconnect(bool erase_credentials) {
    WiFi.disconnect(true, false);
    connected_ = false;
    if (erase_credentials) {
        WifiCredentialsStorage::save("", "");
        ssid_ = "";
        password_ = "";
    }
    notifyWifi("disconnected");
}

void WifiManager::loop() {
    connected_ = (WiFi.status() == WL_CONNECTED);
    if (!connected_ && !ssid_.isEmpty() && next_auto_reconnect_ms_ != 0 && millis() >= next_auto_reconnect_ms_) {
        reconnect(5000);
    }
}

bool WifiManager::isConnected() const {
    return connected_;
}

bool WifiManager::hasCredentials() const {
    if (!ssid_.isEmpty()) {
        return true;
    }
    String ssid;
    String password;
    return WifiCredentialsStorage::load(ssid, password);
}

WifiStatusSnapshot WifiManager::status() const {
    WifiStatusSnapshot snap;
    snap.connected = connected_;
    snap.has_credentials = hasCredentials();
    snap.ssid = connected_ ? WiFi.SSID() : ssid_;
    snap.ip = connected_ ? WiFi.localIP().toString() : String("0.0.0.0");
    snap.rssi = connected_ ? WiFi.RSSI() : 0;
    snap.state = wifiStateToString(WiFi.status());
    return snap;
}

void WifiManager::statusToJson(JsonObject obj) const {
    const WifiStatusSnapshot snap = status();
    obj["connected"] = snap.connected;
    obj["has_credentials"] = snap.has_credentials;
    obj["ssid"] = snap.ssid;
    obj["ip"] = snap.ip;
    obj["rssi"] = snap.rssi;
    obj["state"] = snap.state;
}

void WifiManager::scanToJson(JsonArray arr, int max_networks) const {
    const int count = WiFi.scanNetworks(
        /*async=*/false,
        /*show_hidden=*/false,
        /*passive=*/false,
        /*max_ms_per_chan=*/80);
    const int limit = (max_networks > 0) ? max_networks : 20;
    for (int i = 0; i < count && i < limit; ++i) {
        JsonObject item = arr.add<JsonObject>();
        item["ssid"] = WiFi.SSID(i);
        item["rssi"] = WiFi.RSSI(i);
        item["chan"] = WiFi.channel(i);
        item["enc"] = static_cast<int>(WiFi.encryptionType(i));
    }
    WiFi.scanDelete();
}

bool WifiManager::waitForConnection(uint32_t timeout_ms) {
    const uint32_t start_ms = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start_ms) < timeout_ms) {
        delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
}
