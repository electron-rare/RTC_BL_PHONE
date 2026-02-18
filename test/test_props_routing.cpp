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

void test_props_handle_ping() {
    PropsBridge bridge;
    DynamicJsonDocument doc(64);
    doc["cmd"] = "PING";
    bridge.handleCommand(doc.as<JsonVariantConst>());
    // TODO: brancher executeCommand dans PropsBridge pour vrai test
    TEST_ASSERT_TRUE(true); // Placeholder
}

// setup/loop désactivés pour éviter conflit
