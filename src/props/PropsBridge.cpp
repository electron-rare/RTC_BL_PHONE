#include "PropsBridge.h"

bool PropsBridge::begin() {
    // TODO: Init MQTT/ArduinoProps
    return true;
}

void PropsBridge::tick() {
    // TODO: MQTT loop
}

void PropsBridge::publishStatus(const JsonVariantConst& status) {
    // TODO: Publish status to MQTT
}

void PropsBridge::publishEvent(const JsonVariantConst& event) {
    // TODO: Publish event to MQTT
}

void PropsBridge::handleCommand(const JsonVariantConst& cmd) {
    // TODO: Route command to executeCommand()
}
