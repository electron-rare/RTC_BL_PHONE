#include <unity.h>
#include <ArduinoJson.h>
#include "props/PropsBridge.h"

bool called = false;
String lastCmd;

bool fakeExecuteCommand(const String& cmd, const JsonVariantConst* payloadOpt) {
    called = true;
    lastCmd = cmd;
    return true;
}

// Tests déplacés dans test_main.cpp
