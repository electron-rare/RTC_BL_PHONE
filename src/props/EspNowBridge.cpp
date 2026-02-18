#include "EspNowBridge.h"

bool EspNowBridge::begin() {
    // TODO: Init ESP-NOW
    return true;
}

void EspNowBridge::tick() {
    // TODO: ESP-NOW loop
}

void EspNowBridge::publishStatus(const JsonVariantConst& status) {
    // TODO: Broadcast status via ESP-NOW
}

void EspNowBridge::publishEvent(const JsonVariantConst& event) {
    // TODO: Broadcast event via ESP-NOW
}

void EspNowBridge::handleCommand(const JsonVariantConst& cmd) {
    // TODO: Route command to executeCommand()
}
