# Procédure de tests hardware — RTC_BL_PHONE

## 1. Préparation
- Flasher le firmware compilé sur chaque carte cible :
  - ESP32 DevKitC (esp32dev)
  - ESP32-S3-DevKitC-1 (esp32-s3-devkitc-1)
- Connecter le combiné, SLIC, AG1171S, haut-parleur, micro, carte SD (si lecture audio).
- Ouvrir le moniteur série à 115200 bauds.

## 2. Tests téléphonie
- Émettre un appel via la web UI, MQTT, ESP-NOW, ou commande série.
- Décrocher/raccrocher physiquement (hook), vérifier la détection.
- Recevoir un appel entrant, vérifier la sonnerie et la détection.
- Tester la numérotation DTMF (clavier ou synthétique).

## 3. Tests audio
- Lire un fichier MP3/WAV depuis la carte SD.
- Vérifier le routage audio, le volume, le mute.
- Tester la qualité audio sur haut-parleur et combiné.

## 4. Tests web et MQTT
- Accéder à l’interface web embarquée, vérifier les endpoints (`/`, `/status`, `/config`).
- Envoyer des commandes MQTT (mosquitto_pub), vérifier la réception et l’exécution.
- Vérifier la publication des événements MQTT (mosquitto_sub).

## 5. Tests ESP-NOW
- Envoyer une commande via ESP-NOW depuis un autre ESP32, vérifier la réception et l’exécution.
- Vérifier le broadcast d’événements ESP-NOW.

## 6. Robustesse
- Débrancher/rebrancher le WiFi, vérifier la reconnexion automatique.
- Redémarrer la carte, vérifier la persistance de la config (SPIFFS/NVS).
- Simuler une perte d’agent MQTT/ESP-NOW, vérifier la reprise.

## 7. Logs et validation
- Vérifier les logs série pour chaque action/test.
- Noter tout comportement inattendu ou bug.

## 8. Critères de succès
- Toutes les fonctionnalités principales sont validées sur chaque cible.
- Aucun blocage critique, logs d’erreur documentés.

---

**Version :** 2026-02-18
