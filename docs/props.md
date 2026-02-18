# Intégration ArduinoProps & ESP-NOW

## MQTT (ArduinoProps)
- Contrôle via topics : `rtc_bl_phone/<device_id>/in` (commandes), `rtc_bl_phone/<device_id>/out` (événements).
- Payload JSON : `{ "cmd": "CALL" }`, `{ "cmd": "PLAY", "path": "/welcome.wav" }`, etc.
- Statut publié identique à la sortie série.

## ESP-NOW
- Contrôle local sans broker, même schéma de payload JSON.
- Les événements sont broadcastés à tous les pairs ESP-NOW.

## DTMF logiciel
- Détection Goertzel sur frames audio, publication des chiffres détectés dans les événements MQTT/ESP-NOW.

## Limitations ESP32-S3
- Pas de Bluetooth Classic (BLE uniquement).
- Les fonctionnalités dépendantes du BT Classic sont désactivées sur S3.

## Exemples
- Publier une commande MQTT :
  `mosquitto_pub -t rtc_bl_phone/mondevice/in -m '{"cmd":"CALL"}'`
- Écouter les événements :
  `mosquitto_sub -t rtc_bl_phone/mondevice/out`
