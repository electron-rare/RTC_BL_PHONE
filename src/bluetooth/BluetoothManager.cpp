#include "bluetooth/BluetoothManager.h"
#include "core/AgentSupervisor.h"
#include <Arduino.h>

void notifyBluetooth(const std::string& state, const std::string& error = "") {
    AgentStatus status{state, error, millis()};
    AgentSupervisor::instance().notify("bluetooth", status);
}

BluetoothManager::BluetoothManager()
    : features_(getFeatureMatrix(BoardProfile::ESP32_A252)),
      connected_(false),
      hfp_active_(false),
      ble_active_(false),
      security_enabled_(false) {}

bool BluetoothManager::begin(BoardProfile profile) {
    features_ = getFeatureMatrix(profile);
    connected_ = false;
    hfp_active_ = false;
    ble_active_ = false;
    notifyBluetooth("initialized");
    return true;
}

bool BluetoothManager::connect(const char* mac) {
    if (mac == nullptr || mac[0] == '\0') {
        notifyBluetooth("connect_failed", "invalid mac");
        return false;
    }
    peer_mac_ = mac;
    connected_ = true;
    notifyBluetooth("connected");
    return true;
}

bool BluetoothManager::disconnect() {
    connected_ = false;
    hfp_active_ = false;
    notifyBluetooth("disconnected");
    return true;
}

bool BluetoothManager::isConnected() const {
    return connected_;
}

bool BluetoothManager::startHFP() {
    if (!features_.has_hfp) {
        notifyBluetooth("hfp_failed", "no hfp");
        return false;
    }
    hfp_active_ = true;
    notifyBluetooth("hfp_started");
    return true;
}

bool BluetoothManager::stopHFP() {
    hfp_active_ = false;
    notifyBluetooth("hfp_stopped");
    return true;
}

bool BluetoothManager::startBLE() {
    notifyBluetooth("ble_started");
    if (!features_.has_ble_control) {
        return false;
    }
    ble_active_ = true;
    return true;
}

bool BluetoothManager::stopBLE() {
    notifyBluetooth("ble_stopped");
    ble_active_ = false;
    return true;
}

void BluetoothManager::logStatus() const {
    Serial.printf("[BluetoothManager] connected=%s hfp=%s ble=%s security=%s peer=%s\n",
                  connected_ ? "true" : "false", hfp_active_ ? "true" : "false",
                  ble_active_ ? "true" : "false", security_enabled_ ? "true" : "false",
                  peer_mac_.c_str());
}

void BluetoothManager::statusToJson(JsonObject obj) const {
    obj["connected"] = connected_;
    obj["hfp_active"] = hfp_active_;
    obj["ble_active"] = ble_active_;
    obj["security_enabled"] = security_enabled_;
    obj["peer"] = peer_mac_;
}

void BluetoothManager::setSecurity(bool enabled) {
    security_enabled_ = enabled;
}

bool BluetoothManager::isSecurityEnabled() const {
    return security_enabled_;
}

bool BluetoothManager::isHfpActive() const {
    return hfp_active_;
}

bool BluetoothManager::isBleActive() const {
    return ble_active_;
}

String BluetoothManager::peerMac() const {
    return peer_mac_;
}
