#include <Arduino.h>

/*
  RTC_BL_PHONE - ESP32-S3 + AG1171S control skeleton

  Goal:
  - Provide a practical firmware base for a private analog line project.
  - Keep telephony analog front-end on AG1171S side.
  - Let ESP32 manage call-state logic and debug observability.

  Notes:
  - Pin mapping must be validated against your AG1171S application schematic.
  - Never connect to PSTN without compliant isolation/protection design.
*/

enum class PhoneState : uint8_t {
  ON_HOOK,
  OFF_HOOK,
  DIALING,
  IN_CALL,
  RINGING
};

struct PhonePins {
  uint8_t pinHookSense;
  uint8_t pinRingCmd;
  uint8_t pinLineEnable;
  uint8_t pinLed;
};

#if CONFIG_IDF_TARGET_ESP32S3
constexpr PhonePins PINS{
    .pinHookSense = 4,   // AG1171S hook/off-hook indication input to ESP32-S3
    .pinRingCmd = 5,     // ESP32-S3 output: request ring cadence generator/enable
    .pinLineEnable = 6,  // ESP32-S3 output: line feed enable (through safe driver)
    .pinLed = 48         // ESP32-S3 DevKitC-1 RGB/Status-compatible GPIO
};
#else
constexpr PhonePins PINS{
    .pinHookSense = 27,
    .pinRingCmd = 26,
    .pinLineEnable = 25,
    .pinLed = 2
};
#endif

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t DEBOUNCE_MS = 25;

PhoneState g_state = PhoneState::ON_HOOK;
bool g_hookOffHook = false;
uint32_t g_lastHookEdgeMs = 0;

const char* stateToString(PhoneState state) {
  switch (state) {
    case PhoneState::ON_HOOK: return "ON_HOOK";
    case PhoneState::OFF_HOOK: return "OFF_HOOK";
    case PhoneState::DIALING: return "DIALING";
    case PhoneState::IN_CALL: return "IN_CALL";
    case PhoneState::RINGING: return "RINGING";
  }

  return "UNKNOWN";
}

void setState(PhoneState newState) {
  if (newState == g_state) {
    return;
  }

  g_state = newState;
  Serial.printf("[RTC_PHONE] state=%s\n", stateToString(g_state));

  switch (g_state) {
    case PhoneState::ON_HOOK:
      digitalWrite(PINS.pinLineEnable, LOW);
      digitalWrite(PINS.pinRingCmd, LOW);
      digitalWrite(PINS.pinLed, LOW);
      break;

    case PhoneState::OFF_HOOK:
      digitalWrite(PINS.pinLineEnable, HIGH);
      digitalWrite(PINS.pinRingCmd, LOW);
      digitalWrite(PINS.pinLed, HIGH);
      break;

    case PhoneState::DIALING:
      digitalWrite(PINS.pinLineEnable, HIGH);
      digitalWrite(PINS.pinRingCmd, LOW);
      digitalWrite(PINS.pinLed, HIGH);
      break;

    case PhoneState::IN_CALL:
      digitalWrite(PINS.pinLineEnable, HIGH);
      digitalWrite(PINS.pinRingCmd, LOW);
      digitalWrite(PINS.pinLed, HIGH);
      break;

    case PhoneState::RINGING:
      digitalWrite(PINS.pinLineEnable, HIGH);
      digitalWrite(PINS.pinRingCmd, HIGH);
      digitalWrite(PINS.pinLed, HIGH);
      break;
  }
}

void printHelp() {
  Serial.println("[RTC_PHONE] Commands:");
  Serial.println("  h -> help");
  Serial.println("  r -> enter RINGING");
  Serial.println("  o -> force ON_HOOK");
  Serial.println("  d -> force DIALING");
  Serial.println("  c -> force IN_CALL");
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    const char cmd = static_cast<char>(Serial.read());

    switch (cmd) {
      case 'h':
        printHelp();
        break;
      case 'r':
        setState(PhoneState::RINGING);
        break;
      case 'o':
        setState(PhoneState::ON_HOOK);
        break;
      case 'd':
        setState(PhoneState::DIALING);
        break;
      case 'c':
        setState(PhoneState::IN_CALL);
        break;
      default:
        break;
    }
  }
}

void updateHookState() {
  const bool rawOffHook = (digitalRead(PINS.pinHookSense) == LOW);
  const uint32_t nowMs = millis();

  if (rawOffHook != g_hookOffHook && (nowMs - g_lastHookEdgeMs) > DEBOUNCE_MS) {
    g_lastHookEdgeMs = nowMs;
    g_hookOffHook = rawOffHook;

    Serial.printf("[RTC_PHONE] hook=%s\n", g_hookOffHook ? "OFF_HOOK" : "ON_HOOK");

    if (g_hookOffHook) {
      setState(PhoneState::OFF_HOOK);
    } else {
      setState(PhoneState::ON_HOOK);
    }
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(PINS.pinHookSense, INPUT_PULLUP);
  pinMode(PINS.pinRingCmd, OUTPUT);
  pinMode(PINS.pinLineEnable, OUTPUT);
  pinMode(PINS.pinLed, OUTPUT);

  digitalWrite(PINS.pinRingCmd, LOW);
  digitalWrite(PINS.pinLineEnable, LOW);
  digitalWrite(PINS.pinLed, LOW);

  Serial.println("\n[RTC_PHONE] Boot OK");
#if CONFIG_IDF_TARGET_ESP32S3
  Serial.println("[RTC_PHONE] Target: ESP32-S3");
#else
  Serial.println("[RTC_PHONE] Target: ESP32 (legacy mapping)");
#endif
  Serial.println("[RTC_PHONE] Profile: AG1171S control skeleton");
  printHelp();

  setState(PhoneState::ON_HOOK);
}

void loop() {
  updateHookState();
  handleSerialCommands();
  delay(10);
// Prototype minimal pour valider le câblage d'un téléphone RTC recyclé.
// - Hook switch: détection décroché/raccroché
// - Clavier: lecture en matrice (adaptateur nécessaire selon le modèle)
// - Audio: à implémenter ensuite via codec I2S / interface analogique dédiée

constexpr uint8_t PIN_HOOK = 27;   // Adapter selon le câblage réel
constexpr uint8_t PIN_LED  = 2;    // LED status carte

bool offHook = false;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_HOOK, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  Serial.println("\n[RTC_PHONE] Boot OK");
  Serial.println("[RTC_PHONE] MVP: hook + logique d'etat");
}

void loop() {
  bool hookState = digitalRead(PIN_HOOK) == LOW;  // LOW = décroché (exemple)
  if (hookState != offHook) {
    offHook = hookState;
    digitalWrite(PIN_LED, offHook ? HIGH : LOW);
    Serial.printf("[RTC_PHONE] Etat combine: %s\n", offHook ? "DECROCHE" : "RACCROCHE");
  }

  delay(20);
}
