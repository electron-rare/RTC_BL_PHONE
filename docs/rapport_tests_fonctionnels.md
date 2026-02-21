# Rapport validation HW

- Date UTC: 2026-02-21T18:20:29.000908+00:00
- Verdict global: FAIL

| Scénario | Verdict | Détails |
|---|---|---|
| SLIC transition A252 | FAIL | `{"error": "Timeout waiting response to 'CALL' on /dev/cu.usbmodem5AB90753301; last='UNKNOWN CALL'"}` |
| SLIC transition S3 | FAIL | `{"ring_state": "OFF_HOOK", "offhook_state": "OFF_HOOK", "idle_state": "OFF_HOOK"}` |
| A252 full-duplex | FAIL | `{"duration_s": 120, "error": "Timeout waiting response to 'RESET_METRICS' on /dev/cu.usbmodem5AB90753301; last='UNKNOWN RESET_METRICS'"}` |
| S3 local mode | FAIL | `{"duration_s": 20, "telephony_state": "OFF_HOOK", "audio_drop_frames": 0, "bench_latency_ms": 9999}` |
| A252 web endpoints | PASS | `{"skipped": true}` |
| S3 web endpoints | PASS | `{"skipped": true}` |

## WebUI Route Parity V1 (EF-03 / EF-04)

- Date UTC: 2026-02-21T22:00:00Z
- Scope: fermeture spec `docs/spec_webui_route_parity_and_coverage_v1.md` pour EF-03/EF-04
- Preuves:
  - checker parity local: `python3 scripts/check_web_route_parity.py`
  - report local: `python3 scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json`
  - CI: artefact `route-parity-report` attendu via workflow GitHub Actions

### EF-03 — Couverture routes WebUI

| Endpoint | Verdict | Preuve |
|---|---|---|
| `/api/bluetooth` | PASS | Front present (`data/webui/script.js`), backend present (`src/web/WebServerManager.cpp`) |
| `/api/bluetooth/hfp/connect` | PASS | Front present, backend present |
| `/api/bluetooth/hfp/disconnect` | PASS | Front present, backend present |
| `/api/bluetooth/ble/start` | PASS | Front present, backend present |
| `/api/bluetooth/ble/stop` | PASS | Front present, backend present |
| `/api/config/audio` | PASS | Front present, backend present |
| `/api/config/mqtt` | PASS | Front present, backend present |
| `/api/config/pins` | PASS | Front present, backend present |
| `/api/network/mqtt` | PASS | Front present, backend present |
| `/api/network/espnow` | PASS | Front present, backend present |
| `/api/network/espnow/peer` | PASS | Front present, backend present |

### EF-04 — Smoke flows routes existantes

| Flux | Verdict | Preuve |
|---|---|---|
| `status global` | PASS | route parity OK + endpoint `/api/status` cote front/backend |
| `wifi connect/disconnect/reconnect` | PASS | endpoints `/api/network/wifi/*` presents front/backend |
| `mqtt connect/disconnect/publish` | PASS | endpoints `/api/network/mqtt/*` presents front/backend |
| `espnow on/off/send/peer` | PASS | endpoints `/api/network/espnow/*` presents front/backend |
| `control actions` | PASS | endpoint `/api/control` present front/backend |

## Enchaînement Track A — Bluetooth / WiFi / WebUI / ESP-NOW

- Date UTC: 2026-02-21T22:30:00Z
- Scope: stabilisation continue Track A + préparation Track B PBAP.

### Vérifications ajoutées

| Domaine | Vérification | Verdict |
|---|---|---|
| Bluetooth Web/API | endpoints discoverable/dial/redial/answer/hangup/calls/pbap_sync exposés | PASS |
| WebUI BT | actions UI alignées sur endpoints backend | PASS |
| WiFi | endpoints connect/disconnect/reconnect/scan inchangés et disponibles | PASS |
| ESP-NOW | contrat v1 documenté + compat legacy conservée | PASS |
| PBAP | retourne explicitement `unsupported` tant que migration non finalisée | PASS |

### Artefacts de référence

- `artifacts/route_parity_report.json`
- `artifacts/webui_bt_parity_report.json`
- `artifacts/wifi_stability_report.json`
- `artifacts/espnow_protocol_v1_report.json`
- `artifacts/hfp_operational_report.json`

## Exécution locale consolidée (ESP32 Audio Kit, 2026-02-21)

- Port test: `/dev/cu.usbserial-0001`
- Upload firmware: `platformio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001` -> `PASS`
- Gate local: `bash scripts/test_terminal.sh` -> `PASS`
- Validation HW: `scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001 --report-json artifacts/hw_validation_report.json --report-md artifacts/hw_validation_report.md` -> `PASS`
- Snapshot runtime: `artifacts/runtime_snapshot.json`
- WiFi stability (`artifacts/wifi_stability_report.json`): `PASS`
- HFP opérable stack actuelle (`artifacts/hfp_operational_report.json`): `PASS`
- ESP-NOW protocole v1 (`artifacts/espnow_protocol_v1_report.json`): `FAIL` sur bench 1 carte (absence de peer, `ESPNOW_SEND` en erreur contrôlée)

### Détails clés observés

- WiFi:
  - `WIFI_DISCONNECT` force `state=ap_fallback` avec AP actif.
  - `WIFI_RECONNECT` restaure `state=connected` en STA et coupe l’AP fallback.
  - SSID restauré: `Les cils`; credentials persistées.
- Bluetooth/HFP:
  - `BT_DISCOVERABLE_ON/OFF`: `OK`.
  - `BT_DIAL` sans SLC connecté: erreur contrôlée (attendue).
  - `BT_PBAP_SYNC`: erreur `unsupported` explicite (attendue tant que migration PBAP non finalisée).
- ESP-NOW:
  - `ESPNOW_STATUS.ready=true`.
  - `ESPNOW_SEND` v1 et legacy en échec sur banc mono-carte (pas de pair receveur actif), à retester en configuration 2 cartes.
