# RTC_BL_PHONE Kanban (Live Execution)

## Todo
- Validate hardware gate `incoming GSM -> RTC ring -> BT_ANSWER` on `/dev/cu.usbserial-0001`.
- Validate hardware gate `dial len10 (pulse)` with queued HFP reconnection then auto dial.
- Validate hardware gate `dial len10 (DTMF)` with no false pulse digits.
- Re-run `ESP-NOW` 2-board validation when peer card is available.

## In Progress
- Track `rtos-coex-wifi-bt`:
  - endurance monitor 10 min sans `abort/assert`.
- Track `bt-hfp-stability`:
  - validation E2E entrant GSM -> ring RTC -> décroché -> `BT_ANSWER`.
- Track `webui-runtime`:
  - valider charge SSE en AP fallback + STA connecté.

## Blocked
- `PBAP` contact sync on Arduino ESP32 Bluedroid stack (`BT_PBAP_SYNC` remains `unsupported`).
- Full ESP-NOW E2E protocol proof while second board is unavailable.
- Notion sync automation blocked until MCP Notion OAuth session is reconnected.

## Done
- Added HFP auto reconnect runtime control (`BT_AUTO_RECONNECT_ON/OFF`) and status flag.
- Preserved dial queue logic (`pending_dial_number`) with auto dial after SLC.
- Added `/api/bluetooth/hfp/auto` endpoint (GET status via `BT_STATUS`, POST toggle).
- Added WebUI controls for BT auto reconnect and status line visibility.
- Re-enabled SSE transport server-side and added browser fallback polling.
- Hardened ESP-NOW init to preserve WiFi mode and apply WiFi/BT modem sleep policy.
- Boot order adjusted to initialize WiFi + ESP-NOW before BT stack bring-up.
- Re-asserted WiFi/BT coex policy periodically in `WifiManager::loop()`.
- Added coex policy enforcement in BT stack bring-up (`BluetoothManager::ensureBtStackReady()`).
- Reduced GAP callback logging pressure (guarded by `kVerboseGapLogs=false`).
- Reduced AsyncTCP memory footprint (`STACK=4096`, `QUEUE=12`) to lower RTOS pressure.
- Reordered main loop to run `g_wifi.loop()` before `g_bt.tick()`.
- Updated GitHub issues `#10` and `#11` with latest CI/HW proof links:
  - `https://github.com/electron-rare/RTC_BL_PHONE/issues/10#issuecomment-3941685856`
  - `https://github.com/electron-rare/RTC_BL_PHONE/issues/11#issuecomment-3941685860`

## Run Ledger
- `2026-02-22`:
  - Scope: coex WiFi/BT hardening + HFP autoradio policy + realtime WebUI + docs sync.
  - Target port: `/dev/cu.usbserial-0001`.
  - Firmware hash (sha256): `2c345d5ac459e6aa914f47438cc35bbe33dda68e5a65f41eaf3d8f1efb53e833`.
  - Executed:
    - `platformio run -e esp32dev` -> `PASS`
    - `platformio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001` -> `PASS`
    - `python3 scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001 --report-json artifacts/hw_validation_report.json --report-md artifacts/hw_validation_report.md` -> `PASS`
    - `bash scripts/test_terminal.sh` -> `PASS`
    - `python3 -m unittest scripts/test_check_web_route_parity.py` -> `PASS`
    - `python3 scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json` -> `PASS`
  - Artifacts:
    - `artifacts/hw_validation_report.json`
    - `artifacts/hw_validation_report.md`
    - `artifacts/route_parity_report.json`
