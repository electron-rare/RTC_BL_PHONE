#ifndef BLUETOOTH_I_BLUETOOTH_BACKEND_H
#define BLUETOOTH_I_BLUETOOTH_BACKEND_H

#include <Arduino.h>

#include "core/PlatformProfile.h"

struct PbapContact {
    String display_name;
    String phone_number;
};

class IBluetoothBackend {
public:
    virtual ~IBluetoothBackend() = default;

    virtual bool begin(BoardProfile profile) = 0;
    virtual bool connect(const String& mac) = 0;
    virtual bool disconnect() = 0;

    virtual bool setDiscoverable(bool enabled) = 0;
    virtual bool dial(const String& number) = 0;
    virtual bool redial() = 0;
    virtual bool answer() = 0;
    virtual bool hangup() = 0;
    virtual bool queryCalls() = 0;

    virtual bool syncPbap() = 0;
    virtual bool isPbapSupported() const = 0;
};

#endif  // BLUETOOTH_I_BLUETOOTH_BACKEND_H
