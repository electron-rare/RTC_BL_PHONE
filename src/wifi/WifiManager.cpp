#include "wifi/WifiManager.h"

#include "core/AgentSupervisor.h"
#include "wifi/WifiCredentialsStorage.h"

#include <Arduino.h>

namespace {
constexpr char kFallbackApPrefix[] = "RTC_BL_A252";
constexpr char kFallbackApPassword[] = "rtcblphone";
constexpr uint8_t kFallbackApChannel = 6;
constexpr uint8_t kFallbackApMaxConnections = 4;

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

String wifiModeToString(wifi_mode_t mode) {
    switch (mode) {
        case WIFI_MODE_NULL:
            return "null";
        case WIFI_MODE_STA:
            return "sta";
        case WIFI_MODE_AP:
            return "ap";
        case WIFI_MODE_APSTA:
            return "ap_sta";
        default:
            return "unknown";
    }
}

String buildFallbackApSsid() {
    const uint64_t chip_id = ESP.getEfuseMac();
    const unsigned long suffix = static_cast<unsigned long>(chip_id & 0xFFFFFFULL);
    char name[32];
    snprintf(name, sizeof(name), "%s_%06lX", kFallbackApPrefix, suffix);
    return String(name);
}

}  // namespace

WifiManager::WifiManager()
    : connected_(false),
      ssid_(""),
      password_(""),
      ap_active_(false),
      ap_ssid_(buildFallbackApSsid()),
      ap_password_(kFallbackApPassword),
      next_auto_reconnect_ms_(0),
      reconnect_backoff_ms_(3000) {}

bool WifiManager::begin(const char* ssid, const char* password, uint32_t timeout_ms) {
    return connect(ssid ? String(ssid) : "", password ? String(password) : "", timeout_ms, true);
}

bool WifiManager::connect(const String& ssid, const String& password, uint32_t timeout_ms, bool persist) {
    if (ssid.isEmpty()) {
        connected_ = false;
        notifyWifi("init_failed", "no_ssid");
        startFallbackAp();
        return false;
    }

    ssid_ = ssid;
    password_ = password;

    stopFallbackAp();
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, true);
    delay(100);
    WiFi.begin(ssid_.c_str(), password_.c_str());

    connected_ = waitForConnection(timeout_ms);
    if (connected_) {
        const String link_bssid = WiFi.BSSIDstr();
        const int32_t link_channel = static_cast<int32_t>(WiFi.channel());
        Serial.printf("[WifiManager] STA connected: ssid=%s ip=%s rssi=%d ch=%ld bssid=%s\n",
                      WiFi.SSID().c_str(),
                      WiFi.localIP().toString().c_str(),
                      static_cast<int>(WiFi.RSSI()),
                      static_cast<long>(link_channel),
                      link_bssid.c_str());
        if (persist) {
            WifiCredentialsStorage::save(ssid_, password_);
        }
        notifyWifi("connected");
        next_auto_reconnect_ms_ = 0;
        stopFallbackAp();
    } else {
        notifyWifi("connect_failed");
        next_auto_reconnect_ms_ = millis() + reconnect_backoff_ms_;
        startFallbackAp();
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
    next_auto_reconnect_ms_ = 0;
    if (erase_credentials) {
        WifiCredentialsStorage::save("", "");
        ssid_ = "";
        password_ = "";
    }
    startFallbackAp();
    notifyWifi("disconnected");
}

void WifiManager::loop() {
    connected_ = (WiFi.status() == WL_CONNECTED);
    if (connected_) {
        next_auto_reconnect_ms_ = 0;
        stopFallbackAp();
        return;
    }

    if (!ap_active_) {
        startFallbackAp();
    }

    if (!ssid_.isEmpty() && next_auto_reconnect_ms_ != 0 && millis() >= next_auto_reconnect_ms_) {
        reconnect(5000);
    }
}

void WifiManager::ensureFallbackAp() {
    startFallbackAp();
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
    const bool connected = (WiFi.status() == WL_CONNECTED);
    snap.connected = connected;
    snap.has_credentials = hasCredentials();
    snap.ssid = connected ? WiFi.SSID() : ssid_;
    snap.ip = connected ? WiFi.localIP().toString() : String("0.0.0.0");
    snap.rssi = connected ? WiFi.RSSI() : 0;
    snap.channel = connected ? static_cast<int32_t>(WiFi.channel()) : 0;
    snap.bssid = connected ? WiFi.BSSIDstr() : String("");
    snap.ap_active = ap_active_;
    snap.ap_ssid = ap_active_ ? ap_ssid_ : String("");
    snap.ap_ip = ap_active_ ? WiFi.softAPIP().toString() : String("0.0.0.0");
    snap.mode = wifiModeToString(WiFi.getMode());
    if (connected) {
        snap.state = "connected";
    } else if (snap.ap_active) {
        snap.state = "ap_fallback";
    } else {
        snap.state = wifiStateToString(WiFi.status());
    }
    return snap;
}

void WifiManager::statusToJson(JsonObject obj) const {
    const WifiStatusSnapshot snap = status();
    obj["connected"] = snap.connected;
    obj["has_credentials"] = snap.has_credentials;
    obj["ssid"] = snap.ssid;
    obj["ip"] = snap.ip;
    obj["rssi"] = snap.rssi;
    obj["channel"] = snap.channel;
    obj["bssid"] = snap.bssid;
    obj["state"] = snap.state;
    obj["ap_active"] = snap.ap_active;
    obj["ap_ssid"] = snap.ap_ssid;
    obj["ap_ip"] = snap.ap_ip;
    obj["mode"] = snap.mode;
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

bool WifiManager::startFallbackAp() {
    if (ap_active_) {
        return true;
    }
    if (ap_ssid_.isEmpty()) {
        ap_ssid_ = buildFallbackApSsid();
    }
    if (ap_password_.isEmpty()) {
        ap_password_ = kFallbackApPassword;
    }

    WiFi.mode(WIFI_AP_STA);
    const bool ok = WiFi.softAP(
        ap_ssid_.c_str(),
        ap_password_.c_str(),
        kFallbackApChannel,
        false,
        kFallbackApMaxConnections);

    ap_active_ = ok;
    if (ok) {
        Serial.printf("[WifiManager] fallback AP active: ssid=%s ip=%s\n",
                      ap_ssid_.c_str(),
                      WiFi.softAPIP().toString().c_str());
        notifyWifi("ap_active");
    } else {
        notifyWifi("ap_failed");
    }
    return ok;
}

void WifiManager::stopFallbackAp() {
    if (!ap_active_) {
        return;
    }
    WiFi.softAPdisconnect(true);
    ap_active_ = false;
    notifyWifi("ap_stopped");
}
