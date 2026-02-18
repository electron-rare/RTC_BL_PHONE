# Fiche agent — Stack embarquée (Web, RTOS, Energie, Bluetooth, WiFi)

## Objectif
Décrire l’intégration, la gestion et la validation des stacks web (HTTP), RTOS (FreeRTOS), gestion d’énergie (batterie), Bluetooth et WiFi pour ESP32.

---

### 1. Stack Web (HTTP)
- Librairie : ESPAsyncWebServer
- Serveur HTTP asynchrone, endpoints REST, pages dynamiques.
- Exemple : monitoring, configuration, logs, streaming audio.

### 2. Stack RTOS (FreeRTOS)
- ESP32 embarque FreeRTOS nativement (multi-tâches, priorités, synchronisation).
- Utilisation : tâches audio, gestion hook, serveur web, gestion batterie.
- API : xTaskCreate, vTaskDelay, queues, mutex.

### 3. Gestion d’énergie (batterie)
- Surveillance tension batterie (ADC), gestion deep sleep, wakeup, logs.
- API : analogRead, esp_sleep_enable_ext0_wakeup, esp_deep_sleep_start.
- Scénarios : mode économie, coupure audio, notification web.

### 4. Stack Bluetooth (Classic/BLE)
- Librairie : esp_bt, esp_hf_client_api (HFP), BLE (esp32 BLE Arduino)
- Utilisation : appels HFP, pairing, notifications, streaming audio.
- API : esp_bt_device, esp_bt_main, BLEDevice, BLEServer.

### 5. Stack WiFi
- Librairie : WiFi (Arduino), esp_wifi (ESP-IDF)
- Utilisation : connexion réseau, serveur web, OTA, logs.
- API : WiFi.begin, WiFi.status, esp_wifi_set_mode.

---

## Extension : SLIC, Audio, Lecture Audio, Téléphone SFP

Ajout des stacks SLIC (K50835F, AG1171S), audio (ES8388, PCM5102), lecture audio (MP3/WAV), et stack téléphone SFP pour une gestion complète du hardware téléphonique.

### 6. Stack SLIC (K50835F, AG1171S)
- Gestion interface ligne téléphonique, détection hook, ring, signalisation.
- API : contrôle SLIC, monitoring ligne, gestion appels.

### 7. Stack Audio (ES8388, PCM5102)
- Gestion codec audio, DAC, ADC, routage audio.
- API : configuration ES8388, PCM5102, contrôle volume, monitoring signal.

### 8. Stack Lecture Audio (MP3/WAV)
- Librairie : Audio Tools
- Lecture fichiers audio, streaming, décodage MP3/WAV.
- API : AudioFilePlayer, gestion playlist, contrôle lecture.

### 9. Stack Téléphone SFP
- Gestion appels, signalisation, interface utilisateur.
- API : contrôle SFP, gestion numéro, monitoring état téléphone.

---

**Agent Web** :
- Implémente endpoints HTTP, pages de monitoring/configuration.
- Valide sécurité, logs, tests fonctionnels.

**Agent RTOS** :
- Structure les tâches, priorités, synchronisation.
- Documente l’architecture multitâche.

**Agent Energie** :
- Implémente la surveillance batterie, gestion sleep/wakeup.
- Valide la robustesse, documente les tests.

**Agent Bluetooth** :
- Implémente la gestion HFP, BLE, pairing, streaming.
- Valide la compatibilité, documente les tests.

**Agent WiFi** :
- Implémente la connexion réseau, gestion OTA, logs.
- Valide la robustesse, documente les tests.

**Agent SLIC** :
- Implémente la gestion hardware SLIC, monitoring ligne, signalisation.
- Valide la fiabilité, documente les tests hardware.

**Agent Audio** :
- Implémente la gestion codec audio, routage, monitoring signal.
- Valide la qualité audio, documente les tests.

**Agent Lecture Audio** :
- Implémente la lecture MP3/WAV, playlist, contrôle lecture.
- Valide la robustesse lecture, documente les tests.

**Agent Téléphone SFP** :
- Implémente la gestion appels, interface utilisateur, signalisation.
- Valide la fiabilité téléphone, documente les tests.

**Agent Repo & GitHub** :
- Gère le dépôt, branches, commits, pull requests, releases.
- Automatise CI/CD, synchronisation, documentation des versions.
- Assure la traçabilité, la qualité et la publication du code.

---

**Synergie agents** :
- Web ↔ RTOS ↔ Energie ↔ Bluetooth ↔ WiFi ↔ Firmware ↔ Documentation
- Validation croisée hardware/logiciel.

**Synergie étendue** :
- Repo & GitHub ↔ SLIC ↔ Audio ↔ Lecture Audio ↔ Téléphone SFP ↔ RTOS ↔ Web ↔ Energie ↔ Bluetooth ↔ WiFi ↔ Firmware ↔ Documentation
- Validation croisée hardware/logiciel/audio/téléphonie/gestion de version.

---

**Version :** 2026-02-17