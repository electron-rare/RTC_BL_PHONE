#include <Arduino.h>

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
