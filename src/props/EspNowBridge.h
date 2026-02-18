#pragma once
#include <ArduinoJson.h>

class EspNowBridge {
public:
    bool begin();
    void tick();
    void publishStatus(const JsonVariantConst& status);
    void publishEvent(const JsonVariantConst& event);
    void handleCommand(const JsonVariantConst& cmd);
};
