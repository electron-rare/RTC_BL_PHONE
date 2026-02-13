#include <Arduino.h>

#include <cstdio>

#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_hf_client_api.h"
#include "soc/soc_caps.h"

/*
  RTC_BL_PHONE - ESP32 + AG1171S + Bluetooth HFP

  Livrable:
  - Appel sortant / entrant via HFP
  - Gestion hook (décroché / raccroché)
  - Pilotage ring/line enable pour combiné RTC

  Note matériel:
  - HFP nécessite Bluetooth Classic => ESP32 (pas ESP32-S3)
*/

enum class PhoneState : uint8_t {
  ON_HOOK,
  IDLE,
  RINGING,
  DIALING,
  IN_CALL,
};

struct PhonePins {
  uint8_t hookSense;
  uint8_t ringCmd;
  uint8_t lineEnable;
  uint8_t led;
};

#if CONFIG_IDF_TARGET_ESP32S3
constexpr PhonePins PINS{.hookSense = 4, .ringCmd = 5, .lineEnable = 6, .led = 48};
#else
constexpr PhonePins PINS{.hookSense = 27, .ringCmd = 26, .lineEnable = 25, .led = 2};
#endif

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t DEBOUNCE_MS = 25;
constexpr char DEVICE_NAME[] = "RTC_BL_PHONE";
constexpr char DEFAULT_PEER_ADDR[] = "00:00:00:00:00:00";  // A remplacer ou via commande "p <mac>".

PhoneState g_state = PhoneState::ON_HOOK;
bool g_hookOffHook = false;
bool g_hfpReady = false;
bool g_hfpConnected = false;
bool g_audioConnected = false;
bool g_callActive = false;
bool g_callIncoming = false;
bool g_callSetupOutgoing = false;
uint32_t g_lastHookEdgeMs = 0;
String g_serialLine;
String g_peerAddrString = DEFAULT_PEER_ADDR;
esp_bd_addr_t g_peerAddr = {0};

const char* stateToString(PhoneState state) {
  switch (state) {
    case PhoneState::ON_HOOK: return "ON_HOOK";
    case PhoneState::IDLE: return "IDLE";
    case PhoneState::RINGING: return "RINGING";
    case PhoneState::DIALING: return "DIALING";
    case PhoneState::IN_CALL: return "IN_CALL";
  }
  return "UNKNOWN";
}

bool parseBdAddr(const char* mac, esp_bd_addr_t out) {
  unsigned int v[6] = {0};
  if (sscanf(mac, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
    return false;
  }

  for (int i = 0; i < 6; ++i) {
    if (v[i] > 0xFF) {
      return false;
    }
    out[i] = static_cast<uint8_t>(v[i]);
  }
  return true;
}

bool updatePeerAddr(const String& addr) {
  esp_bd_addr_t parsed = {0};
  if (!parseBdAddr(addr.c_str(), parsed)) {
    return false;
  }
  memcpy(g_peerAddr, parsed, sizeof(g_peerAddr));
  g_peerAddrString = addr;
  return true;
}

bool isPeerAddrConfigured() {
  return g_peerAddrString != DEFAULT_PEER_ADDR;
}

void applyOutputsForState() {
  switch (g_state) {
    case PhoneState::ON_HOOK:
      digitalWrite(PINS.lineEnable, LOW);
      digitalWrite(PINS.ringCmd, LOW);
      digitalWrite(PINS.led, LOW);
      break;
    case PhoneState::IDLE:
      digitalWrite(PINS.lineEnable, HIGH);
      digitalWrite(PINS.ringCmd, LOW);
      digitalWrite(PINS.led, HIGH);
      break;
    case PhoneState::RINGING:
      digitalWrite(PINS.lineEnable, HIGH);
      digitalWrite(PINS.ringCmd, HIGH);
      digitalWrite(PINS.led, HIGH);
      break;
    case PhoneState::DIALING:
    case PhoneState::IN_CALL:
      digitalWrite(PINS.lineEnable, HIGH);
      digitalWrite(PINS.ringCmd, LOW);
      digitalWrite(PINS.led, HIGH);
      break;
  }
}

void setState(PhoneState newState) {
  if (g_state == newState) {
    return;
  }
  g_state = newState;
  applyOutputsForState();
  Serial.printf("[RTC_PHONE] state=%s\n", stateToString(g_state));
}

void refreshPhoneState() {
  if (!g_hookOffHook) {
    setState(PhoneState::ON_HOOK);
  } else if (g_callIncoming && !g_callActive) {
    setState(PhoneState::RINGING);
  } else if (g_callSetupOutgoing && !g_callActive) {
    setState(PhoneState::DIALING);
  } else if (g_callActive) {
    setState(PhoneState::IN_CALL);
  } else {
    setState(PhoneState::IDLE);
  }
}

void printHelp() {
  Serial.println("[RTC_PHONE] Commandes:");
  Serial.println("  h             -> help");
  Serial.println("  s             -> status");
  Serial.println("  p <mac>       -> set peer MAC HFP (AA:BB:CC:DD:EE:FF)");
  Serial.println("  b             -> connect HFP AG");
  Serial.println("  x             -> disconnect HFP AG");
  Serial.println("  a             -> answer incoming");
  Serial.println("  e             -> end/reject call");
  Serial.println("  m <number>    -> dial number");
  Serial.println("  v <0..15>     -> set speaker volume");
}

void printStatus() {
  Serial.printf("[RTC_PHONE] hook=%s state=%s ready=%s hfp=%s audio=%s incoming=%s outgoing=%s active=%s peer=%s\n",
                g_hookOffHook ? "OFF_HOOK" : "ON_HOOK", stateToString(g_state), g_hfpReady ? "YES" : "NO",
                g_hfpConnected ? "YES" : "NO", g_audioConnected ? "YES" : "NO", g_callIncoming ? "YES" : "NO",
                g_callSetupOutgoing ? "YES" : "NO", g_callActive ? "YES" : "NO", g_peerAddrString.c_str());
}

#if SOC_BT_CLASSIC_SUPPORTED
void onCallTerminatedByState() {
  if (!g_callActive && !g_callSetupOutgoing) {
    g_audioConnected = false;
  }
}

void hfpCallback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param) {
  switch (event) {
    case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
      g_hfpConnected = (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED);
      if (!g_hfpConnected) {
        g_audioConnected = false;
      }
      Serial.printf("[HFP] conn_state=%d\n", param->conn_stat.state);
      break;

    case ESP_HF_CLIENT_AUDIO_STATE_EVT:
      g_audioConnected = (param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED ||
                          param->audio_stat.state == ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC);
      Serial.printf("[HFP] audio_state=%d\n", param->audio_stat.state);
      break;

    case ESP_HF_CLIENT_RING_IND_EVT:
      g_callIncoming = true;
      Serial.println("[HFP] incoming ring");
      break;

    case ESP_HF_CLIENT_CALL_IND_EVT:
      g_callActive = (param->call.ind != 0);
      Serial.printf("[HFP] call=%d\n", param->call.ind);
      onCallTerminatedByState();
      break;

    case ESP_HF_CLIENT_CALL_SETUP_IND_EVT:
      g_callSetupOutgoing = (param->call_setup.status == ESP_HF_CALL_SETUP_STATUS_OUTGOING_DIALING ||
                             param->call_setup.status == ESP_HF_CALL_SETUP_STATUS_OUTGOING_ALERTING);
      if (param->call_setup.status == ESP_HF_CALL_SETUP_STATUS_INCOMING) {
        g_callIncoming = true;
      } else if (param->call_setup.status == ESP_HF_CALL_SETUP_STATUS_IDLE) {
        g_callIncoming = false;
      }
      Serial.printf("[HFP] call_setup=%d\n", param->call_setup.status);
      onCallTerminatedByState();
      break;

    case ESP_HF_CLIENT_CLIP_EVT:
      if (param->clip.number) {
        Serial.printf("[HFP] caller=%s\n", param->clip.number);
      }
      break;

    default:
      break;
  }

  refreshPhoneState();
}

bool initHfp() {
  if (!updatePeerAddr(g_peerAddrString)) {
    Serial.println("[HFP] MAC invalide. Utilisez: p AA:BB:CC:DD:EE:FF");
    return false;
  }

  esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[HFP] mem_release failed: %s\n", esp_err_to_name(err));
    return false;
  }

  const esp_bt_controller_config_t btCfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  err = esp_bt_controller_init(&btCfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[HFP] bt_controller_init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[HFP] bt_controller_enable failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_bluedroid_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[HFP] bluedroid_init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_bluedroid_enable();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[HFP] bluedroid_enable failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_bt_dev_set_device_name(DEVICE_NAME);
  if (err != ESP_OK) {
    Serial.printf("[HFP] set_device_name failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_hf_client_register_callback(hfpCallback);
  if (err != ESP_OK) {
    Serial.printf("[HFP] register_callback failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_hf_client_init();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[HFP] init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  g_hfpReady = true;
  Serial.println("[HFP] stack ready");
  if (!isPeerAddrConfigured()) {
    Serial.println("[HFP] ATTENTION: configurez le peer avec p <mac> avant b");
  }
  return true;
}

void connectHfp() {
  if (!g_hfpReady) {
    Serial.println("[HFP] stack non initialisee");
    return;
  }
  if (!isPeerAddrConfigured()) {
    Serial.println("[HFP] peer MAC non configure. Utilisez: p <mac>");
    return;
  }
  const esp_err_t err = esp_hf_client_connect(g_peerAddr);
  Serial.printf("[HFP] connect -> %s\n", esp_err_to_name(err));
}

void disconnectHfp() {
  if (!g_hfpReady) {
    Serial.println("[HFP] stack non initialisee");
    return;
  }
  const esp_err_t err = esp_hf_client_disconnect(g_peerAddr);
  Serial.printf("[HFP] disconnect -> %s\n", esp_err_to_name(err));
}

void answerCall() {
  if (!g_hfpConnected || !g_callIncoming) {
    Serial.println("[HFP] aucun appel entrant a decrocher");
    return;
  }
  const esp_err_t err = esp_hf_client_answer_call();
  Serial.printf("[HFP] answer -> %s\n", esp_err_to_name(err));
}

void endCall() {
  if (!g_hfpConnected) {
    Serial.println("[HFP] non connecte");
    return;
  }

  esp_err_t err = ESP_FAIL;
  if (g_callIncoming && !g_callActive) {
    err = esp_hf_client_reject_call();
    Serial.printf("[HFP] reject -> %s\n", esp_err_to_name(err));
  } else {
    err = esp_hf_client_terminate_call();
    Serial.printf("[HFP] terminate -> %s\n", esp_err_to_name(err));
  }
}

void dialNumber(const String& number) {
  if (!g_hfpConnected) {
    Serial.println("[HFP] non connecte");
    return;
  }
  if (number.length() == 0) {
    Serial.println("[HFP] numero vide");
    return;
  }
  const esp_err_t err = esp_hf_client_dial(number.c_str());
  Serial.printf("[HFP] dial(%s) -> %s\n", number.c_str(), esp_err_to_name(err));
}

void setSpeakerVolume(int value) {
  if (!g_hfpConnected) {
    Serial.println("[HFP] non connecte");
    return;
  }
  const int clipped = constrain(value, 0, 15);
  const esp_err_t err = esp_hf_client_volume_update(ESP_HF_VOLUME_CONTROL_TARGET_SPK, clipped);
  Serial.printf("[HFP] volume=%d -> %s\n", clipped, esp_err_to_name(err));
}
#else
bool initHfp() {
  Serial.println("[HFP] indisponible: cible sans Bluetooth Classic (ex: ESP32-S3)");
  return false;
}

void connectHfp() { Serial.println("[HFP] non supporte sur cette cible"); }
void disconnectHfp() { Serial.println("[HFP] non supporte sur cette cible"); }
void answerCall() { Serial.println("[HFP] non supporte sur cette cible"); }
void endCall() { Serial.println("[HFP] non supporte sur cette cible"); }
void dialNumber(const String&) { Serial.println("[HFP] non supporte sur cette cible"); }
void setSpeakerVolume(int) { Serial.println("[HFP] non supporte sur cette cible"); }
#endif

void executeCommand(const String& line) {
  if (line == "h") {
    printHelp();
  } else if (line == "s") {
    printStatus();
  } else if (line == "b") {
    connectHfp();
  } else if (line == "x") {
    disconnectHfp();
  } else if (line == "a") {
    answerCall();
  } else if (line == "e") {
    endCall();
  } else if (line.startsWith("m ")) {
    dialNumber(line.substring(2));
  } else if (line.startsWith("v ")) {
    setSpeakerVolume(line.substring(2).toInt());
  } else if (line.startsWith("p ")) {
    const String mac = line.substring(2);
    if (updatePeerAddr(mac)) {
      Serial.printf("[HFP] peer configure: %s\n", g_peerAddrString.c_str());
    } else {
      Serial.println("[HFP] format MAC invalide. Ex: AA:BB:CC:DD:EE:FF");
    }
  } else if (!line.isEmpty()) {
    Serial.printf("[RTC_PHONE] commande inconnue: %s\n", line.c_str());
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      executeCommand(g_serialLine);
      g_serialLine = "";
    } else {
      g_serialLine += c;
    }
  }
}

void updateHookState() {
  const bool rawOffHook = (digitalRead(PINS.hookSense) == LOW);
  const uint32_t nowMs = millis();

  if (rawOffHook != g_hookOffHook && (nowMs - g_lastHookEdgeMs) > DEBOUNCE_MS) {
    g_lastHookEdgeMs = nowMs;
    g_hookOffHook = rawOffHook;
    Serial.printf("[RTC_PHONE] hook=%s\n", g_hookOffHook ? "OFF_HOOK" : "ON_HOOK");

    if (!g_hookOffHook && (g_callActive || g_callSetupOutgoing || g_callIncoming)) {
      endCall();
    } else if (g_hookOffHook && g_callIncoming && !g_callActive) {
      answerCall();
    }

    refreshPhoneState();
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PINS.hookSense, INPUT_PULLUP);
  pinMode(PINS.ringCmd, OUTPUT);
  pinMode(PINS.lineEnable, OUTPUT);
  pinMode(PINS.led, OUTPUT);

  digitalWrite(PINS.ringCmd, LOW);
  digitalWrite(PINS.lineEnable, LOW);
  digitalWrite(PINS.led, LOW);

  Serial.println("\n[RTC_PHONE] Boot OK");
#if CONFIG_IDF_TARGET_ESP32S3
  Serial.println("[RTC_PHONE] Target: ESP32-S3");
#else
  Serial.println("[RTC_PHONE] Target: ESP32");
#endif
  Serial.println("[RTC_PHONE] Profile: AG1171S + Bluetooth HFP");

  printHelp();
  g_hookOffHook = (digitalRead(PINS.hookSense) == LOW);
  initHfp();
  refreshPhoneState();
}

void loop() {
  updateHookState();
  handleSerialCommands();
  delay(10);
}
