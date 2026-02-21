# Rapport campagne globale de tests fonctionnels — RTC_BL_PHONE

## Résumé global
Tous les modules ont été testés individuellement et en intégration. Les résultats sont détaillés par module ci-dessous.

---

### AudioManager
- Initialisation : OK
- Abstraction codec (ES8388, PCM5102, Generic) : OK
- Volume/mute : OK, tests edge cases
- Monitoring signal : OK
- Logs : conformes, traçabilité assurée
- Robustesse : aucun crash, gestion d’erreur efficace

### RTOSManager
- Multitâche : création et gestion de tâches FreeRTOS OK
- Watchdog : activation, feed, timeout OK
- Audit des tâches : reporting complet
- Logs : conformes, traçabilité assurée
- Robustesse : aucun deadlock, timing stable

### BluetoothManager
- Initialisation : OK
- Connexion/déconnexion : OK (HFP, BLE)
- Sécurité : activation/désactivation OK
- Logs : conformes, traçabilité assurée
- Robustesse : gestion des erreurs, fallback ESP32-S3 OK

### Endpoints HTTP
- Tests GET/POST sur tous les endpoints : OK
- Sécurisation (authentification, validation) : OK
- Charge et scénarios multi-utilisateurs : OK
- Logs et artefacts centralisés

### Sécurité
- Authentification endpoints : OK
- Validation des données : OK
- Tests d’intrusion : aucun accès non autorisé
- Traçabilité : logs et artefacts CI

---

## Conclusion
Tous les modules sont validés, robustes et intégrés. La traçabilité, la sécurité et la documentation sont assurées. Prêt pour la phase suivante ou la livraison.

**Version :** 2026-02-17

---

## Campagne terrain ZeroClaw — 2026-02-21 (ESP32 audio dev)

Contexte:

- Cible active: `esp32dev` (ESP32 audio dev)
- Port flash: `/dev/cu.SLAB_USBtoUART`
- Scope: terminal build/flash/smoke, sans carte S3 sur ce bench

Resultats:

- Preflight ZeroClaw USB: OK
- Build `pio run -e esp32dev`: OK
- Upload `pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART`: OK
- Smoke serie:
  - `PING` => `PONG` (OK)
  - `STATUS` => JSON complet (OK)

Incident corrige pendant la campagne:

- Reboot loop `lwIP Invalid mbox` au demarrage web.
- Cause: `AsyncWebServer::begin()` lance avant init reseau.
- Correctif applique:
  - init Wi-Fi AP (`RTC_BL_PHONE`) avant `g_web.begin()`
  - correction payload `STATUS` (`doc.as<JsonObject>()` au lieu de `doc.to<JsonObject>()`)

Verdict campagne 2026-02-21:

- PASS pour le flux `esp32dev` en terminal.
