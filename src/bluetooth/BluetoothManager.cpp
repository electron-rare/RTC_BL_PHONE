#include "bluetooth/BluetoothManager.h"

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
    return true;
}

bool BluetoothManager::connect(const char* mac) {
    if (mac == nullptr || mac[0] == '\0') {
        return false;
    }
    peer_mac_ = mac;
    connected_ = true;
    return true;
}

bool BluetoothManager::disconnect() {
    connected_ = false;
    hfp_active_ = false;
    return true;
}

bool BluetoothManager::isConnected() const {
    return connected_;
}

bool BluetoothManager::startHFP() {
    if (!features_.has_hfp) {
        return false;
    }
    hfp_active_ = true;
    return true;
}

bool BluetoothManager::stopHFP() {
    hfp_active_ = false;
    return true;
}

bool BluetoothManager::startBLE() {
    if (!features_.has_ble_control) {
        return false;
    }
    ble_active_ = true;
    return true;
}

bool BluetoothManager::stopBLE() {
    ble_active_ = false;
    return true;
}

void BluetoothManager::logStatus() const {
    Serial.printf("[BluetoothManager] connected=%s hfp=%s ble=%s security=%s peer=%s\n",
                  connected_ ? "true" : "false", hfp_active_ ? "true" : "false",
                  ble_active_ ? "true" : "false", security_enabled_ ? "true" : "false",
                  peer_mac_.c_str());
}

void BluetoothManager::setSecurity(bool enabled) {
    security_enabled_ = enabled;
}

bool BluetoothManager::isSecurityEnabled() const {
    return security_enabled_;
}
