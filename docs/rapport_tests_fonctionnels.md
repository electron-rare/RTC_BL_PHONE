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
