#ifndef BLUETOOTH_BLUETOOTH_MANAGER_H
#define BLUETOOTH_BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#include "core/PlatformProfile.h"

class BluetoothManager {
public:
    BluetoothManager();
    bool begin(BoardProfile profile);
    bool connect(const char* mac);
    bool disconnect();
    bool isConnected() const;
    bool startHFP();
    bool stopHFP();
    bool startBLE();
    bool stopBLE();
    void logStatus() const;
    void statusToJson(JsonObject obj) const;
    void setSecurity(bool enabled);
    void setBleCommandHandler(std::function<String(const String&)> handler);
    void publishBleStatus();
    String executeBleCommand(const String& cmd);
    void handleHfpEvent(int event, const void* param);
    void onBleClientConnected(bool connected);
    bool isSecurityEnabled() const;
    bool isHfpActive() const;
    bool isBleActive() const;
    String peerMac() const;

private:
    bool ensureBtStackReady();
    bool ensureHfpClientReady();
    bool parseMac(const String& mac, uint8_t out[6]) const;
    String formatMac(const uint8_t* mac) const;

    FeatureMatrix features_;
    bool stack_ready_;
    bool hfp_initialized_;
    bool hfp_requested_;
    bool ble_stack_initialized_;
    bool ble_service_ready_;
    bool ble_client_connected_;
    bool connected_;
    bool hfp_active_;
    bool ble_active_;
    bool security_enabled_;
    String peer_mac_;
    uint8_t peer_addr_[6];
    bool peer_addr_valid_;
    String last_hfp_event_;
    String last_ble_event_;
    String last_error_;
    String ble_last_command_;
    String ble_last_response_;
    std::function<String(const String&)> ble_command_handler_;
};

#endif  // BLUETOOTH_BLUETOOTH_MANAGER_H
