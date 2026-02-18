#ifndef BLUETOOTH_BLUETOOTH_MANAGER_H
#define BLUETOOTH_BLUETOOTH_MANAGER_H

#include <Arduino.h>

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
    void setSecurity(bool enabled);
    bool isSecurityEnabled() const;

private:
    FeatureMatrix features_;
    bool connected_;
    bool hfp_active_;
    bool ble_active_;
    bool security_enabled_;
    String peer_mac_;
};

#endif  // BLUETOOTH_BLUETOOTH_MANAGER_H
