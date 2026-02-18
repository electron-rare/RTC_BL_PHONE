
# Spécifications des stacks agents RTC_BL_PHONE

## AudioManager
- Initialisation audio, lecture de fichiers, capture, métriques runtime.
- Méthodes : `begin`, `playFile`, `startCapture`, `stopCapture`, `metrics`, `resetMetrics`.
- Doit notifier le superviseur à chaque changement d’état ou erreur critique.

## BluetoothManager
- Gestion BT Classic et BLE, connexion/déconnexion, sécurité, logs d’état.
- Méthodes : `begin`, `connect`, `disconnect`, `isConnected`, `startHFP`, `stopHFP`, `startBLE`, `stopBLE`, `logStatus`, `setSecurity`.
- Doit notifier le superviseur à chaque changement d’état ou erreur critique.

## RTOSManager
- Gestion des tâches FreeRTOS, watchdog, audit, logs d’état.
- Méthodes : `begin`, `createTask`, `startScheduler`, `auditTasks`, `logStatus`, `enableWatchdog`, `feedWatchdog`.
- Doit notifier le superviseur à chaque événement critique (ex : watchdog déclenché).

## SLICManager
- Encapsulation du contrôleur SLIC, gestion des états ligne, transitions, monitoring.
- Méthodes : `begin`, `attachController`, `monitorLine`, `controlCall`, `state`, `isHookOff`.
- Doit notifier le superviseur à chaque changement d’état ou erreur critique.

## WifiManager
- Gestion connexion WiFi, état, SSID, reconnexion, logs.
- Méthodes : `begin`, `loop`, `isConnected`, `scan`, `reconnect`, `logStatus`.
- Doit notifier le superviseur à chaque changement d’état ou erreur critique.

---

# Améliorations évidentes
- Ajout du pattern de notification vers AgentSupervisor dans chaque stack (méthode notify à chaque changement d’état ou erreur).
- Ajout d’un bus d’événements pour permettre la réaction croisée (ex : coupure WiFi → pause audio).
- Exposition de l’état global via web API et logs synthétiques.
- Ajout de tests de coordination dans la validation Python.

---

# Audit
- Toutes les stacks sont présentes et conformes à leur rôle métier.
- La coordination dynamique (supervision, réaction croisée) est en cours d’amélioration avec AgentSupervisor.
- Les spécifications sont désormais explicites et à jour.
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