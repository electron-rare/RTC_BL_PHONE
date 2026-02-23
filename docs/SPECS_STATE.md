# État des specs RTC_BL_PHONE

## 1) Index des specs

- `docs/spec_webui_route_parity_and_coverage_v1.md` (spécification principale active)
- `docs/spec_bt_hfp_pbap_dialing_v1.md` (nouvelle spec téléphonie Bluetooth, statut `Draft`)
- Notion hub: `RTC_BL_PHONE Delivery Hub`
- Notion spec mirror: `RTC_BL_PHONE - Spec - WebUI Route Parity v1`
- Notion plan: `RTC_BL_PHONE - Plan d'implementation - Route Parity Closure`
- Notion tasks DB: `RTC_BL_PHONE - Tasks` (`collection://89fa36aa-8933-4294-a922-90c6c34d5130`)

## 2) Bilan opérationnel de la spec webui route parity

| Axe | Statut | Preuve / commentaire |
|---|---|---|
| EF-01 — Gate route parity en CI | Done | Workflow CI exécute le checker parity avec export JSON. |
| EF-02 — Artefact de preuve | Done | Rapport JSON généré (`artifacts/route_parity_report.json`) et upload CI via `route-parity-report`. |
| EF-03 — Couverture routes WebUI | Done | Couverture documentée et validée dans `docs/rapport_tests_fonctionnels.md` (section WebUI Route Parity V1). |
| EF-04 — Non-régression des routes existantes | Done | Smoke flows routes tracés dans `docs/rapport_tests_fonctionnels.md` et cohérents avec parity checker. |

## 3) État global de consolidation

- `platformio run -e esp32dev` : OK
- `platformio test --without-uploading --without-testing -e esp32dev` : OK
- `scripts/check_web_route_parity.py` (local) : backend routes 39, frontend routes 38, check passé
- `scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json` : OK (report généré)
- `platformio run -e esp32-s3-devkitc-1` : échec connu sur liens Bluetooth/HFP externes (suivi séparé)

## 3-bis) État spec BT HFP/PBAP/Numérotation

- HFP commande-level: OK (connect/disconnect commandes acceptées).
- HFP call-control AT: implémenté (`BT_DIAL`/`DIAL`, `BT_REDIAL`, `BT_ANSWER`, `BT_HANGUP`, `BT_CALLS`).
- Discoverable policy: OK (si aucun client BT connecté, `discoverable=true` forcé; validation locale via `BT_STATUS` avant/après `BT_DISCOVERABLE_OFF`).
- HFP liaison réelle: KO sur tests assistés (pas de montée `connected=true`/`hfp_active=true`).
- PBAP: bloqué sur stack actuelle (`arduino-esp32` bluedroid sans API PBAP exposée côté firmware), `BT_PBAP_SYNC` retourne `unsupported`.
- Numérotation HFP/AT: implémentée côté firmware, validation E2E téléphone encore à obtenir (dépend de la liaison HFP réelle).

## 4) Lacunes à combler pour passer la spec en `DONE`

1. Conserver le suivi S3 Bluetooth/HFP hors scope parity V1.

## 5) Risques actifs

- Écart documentation / exécution (doD non uniformément tracée)
- Dépendances CI sur les branches de support non conservées (si nettoyage de branches engagé)
- Blocs matériels/firmware S3 (liens Bluetooth/HFP) pouvant masquer la visibilité produit

## 6) Enchaînement Track A / Track B (Bluetooth, WiFi, WebUI, ESP-NOW)

### Track A — Delivery rapide (état courant)

- WiFi:
  - endpoints Web/API présents: `/api/network/wifi`, `/connect`, `/disconnect`, `/reconnect`, `/scan`.
  - fallback AP actif en perte STA (`state=ap_fallback`).
- WebUI Bluetooth:
  - endpoints étendus pour discoverable, dial/redial/answer/hangup/calls, pbap sync.
  - affichage états BT enrichis (`call_state`, `slc_connected`, `pbap_supported`).
- ESP-NOW:
  - protocole v1 documenté (`docs/espnow_api_v1.md`).
  - compat legacy conservée (`cmd/raw/command/action`).
- Bluetooth HFP:
  - commandes d'appel opérationnelles.
  - PBAP toujours `unsupported` sur stack actuelle (comportement explicitement tracé).

### Track B — Migration BT/PBAP (préparation)

- Faisabilité documentée: `docs/bt_pbap_migration_feasibility.md`
- Build migration préparé: `env:esp32dev-bt-migrate`
- Interface d'abstraction ajoutée: `src/bluetooth/IBluetoothBackend.h`

### Gates de validation

- Gate local minimal:
  - `platformio run -e esp32dev`
  - `bash scripts/test_terminal.sh`
  - `scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001`
- Gate CI:
  - workflows `Repo State` + `Firmware CI` verts sur `main`

## 7) Exécution hardware locale (2026-02-21, ESP32 Audio Kit)

- Cible testée: `/dev/cu.usbserial-0001`
- Upload firmware: `platformio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001` -> `PASS`
- Gate local: `bash scripts/test_terminal.sh` -> `PASS` (build unit test, host dtmf, parity parser, parity routes)
- Smoke HW: `python3 scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001 --report-json artifacts/hw_validation_report.json --report-md artifacts/hw_validation_report.md` -> `PASS`
- Snapshot runtime: `artifacts/runtime_snapshot.json` (inclut `BT_STATUS`, `WIFI_STATUS`, `ESPNOW_STATUS`, `/api/status`)
- WiFi stability: `artifacts/wifi_stability_report.json` -> `PASS` (disconnect => `ap_fallback`, reconnect => `connected`, credentials persistées)
- HFP opérable (stack actuelle): `artifacts/hfp_operational_report.json` -> `PASS` (discoverable on/off ok, `BT_DIAL` sans SLC en erreur contrôlée, `BT_PBAP_SYNC unsupported` explicite)
- ESP-NOW protocole v1: `artifacts/espnow_protocol_v1_report.json` -> `FAIL` en bench 1 carte (pas de peer radio actif, `ESPNOW_SEND` retourne `ERR`)
- Décision de gate: WiFi/WebUI/Bluetooth HFP command-level `GO`; ESP-NOW E2E v1 two-board `BLOCKED` jusqu’à disponibilité d’une 2e carte

### Addendum (2026-02-22)

- Firmware reflash `esp32dev` sur `/dev/cu.usbserial-0001` effectué.
- Validation BT ready-for-pairing: `artifacts/bt_hfp_readiness.json`
  - `discoverable=true` sans client connecté.
  - `BT_CALLS`/`BT_DIAL` en erreur contrôlée hors SLC.
  - `BT_PBAP_SYNC` retourne `ERR ... unsupported` (attendu).
- Validation discoverable forcé: `artifacts/bt_discoverable_policy_check.json`
  - `BT_DISCOVERABLE_OFF` accepté mais état final reste `discoverable=true` tant que non connecté.
- ESP-NOW (tests minimisés): `artifacts/espnow_peer91_probe_clean.json`
  - peer `10:20:BA:58:C7:48` ajouté.
  - envoi unitaire observé avec `tx_fail=1` côté A252 (non bloquant pour le track Bluetooth).

### Addendum (2026-02-22 — wiring/polarité/audio)

- Polarité `GPIO21` verrouillée: `AMP_EN` actif bas (`LOW=ON`, `HIGH=OFF`).
- `SLIC LINE` retiré du runtime (broche non utilisée en bench courant).
- Mapping bench confirmé:
  - `RM=GPIO18`, `FR=GPIO5`, `SHK=GPIO23`, `PD=GPIO19`.
- Tonalité locale fixée à `425 Hz` et niveau augmenté (boost léger) sans changement de timbre.
- Durcissement Bluetooth en cours:
  - reprise explicite sur `ACL_DISCONN`,
  - retry audio SCO piloté par état d'appel (`dialing/alerting/ringing/active`).

### Addendum (2026-02-22 — dial/ring policy BT)

- Spécification ajoutée:
  - `DIAL`/`BT_DIAL` doit accepter une demande même sans SLC immédiat:
    - queue numéro,
    - déclenche/reprend connexion HFP,
    - compose automatiquement après `SLC_CONNECTED`.
  - Appel entrant GSM via HFP (`call_state=ringing`) doit déclencher la sonnerie RTC.
  - Décroché RTC en phase sonnerie doit déclencher `BT_ANSWER`.
- Précondition produit explicitée:
  - sans lien HFP actif, la sonnerie RTC sur appel GSM n'est pas possible;
  - mitigation firmware: auto-reconnect sur peer bondé + discoverable forcé si non connecté.
- Statut:
  - implémentation firmware: `in_progress` (queue dial + auto reconnect renforcés),
  - validation hardware E2E appel entrant/sortant: `pending`.

### Addendum (2026-02-22 — autonomous execution batch)

- Runtime policy applied:
  - HFP auto reconnect remains enabled by default (`auto_reconnect_enabled=true`).
  - New runtime toggles available:
    - `BT_AUTO_RECONNECT_ON`
    - `BT_AUTO_RECONNECT_OFF`
  - `BT_STATUS` now exposes `auto_reconnect_enabled`.
- Web/API alignment:
  - endpoint added: `POST /api/bluetooth/hfp/auto` with body `{"enabled":true|false}`.
  - endpoint readback: `GET /api/bluetooth/hfp/auto` via `BT_STATUS`.
  - SSE realtime path re-enabled (`/api/events`) with client polling fallback.
- Coexistence hardening:
  - ESP-NOW init now preserves current WiFi mode and applies modem sleep policy for WiFi/BT coexistence.
  - setup order adjusted: WiFi + ESP-NOW before BT startup.
- Tracking:
  - `docs/AGENT_TODO.md` converted to live kanban with run ledger.

### Addendum (2026-02-22 — coex/runtime hardening batch 2)

- Build + flash:
  - `platformio run -e esp32dev` -> `PASS`
  - `platformio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001` -> `PASS`
- Runtime/bench:
  - `python3 scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001 --report-json artifacts/hw_validation_report.json --report-md artifacts/hw_validation_report.md` -> `PASS`
  - firmware hash (sha256): `2c345d5ac459e6aa914f47438cc35bbe33dda68e5a65f41eaf3d8f1efb53e833`
- Coexistence hardening applied:
  - réimposition périodique `WIFI_PS_MIN_MODEM` dans `WifiManager::loop()`.
  - réimposition coex avant/après `btStart()` dans `BluetoothManager::ensureBtStackReady()`.
  - logs GAP en callback réduits (`kVerboseGapLogs=false`) pour éviter contention newlib/locks en task BT.
  - pression mémoire AsyncTCP réduite (`STACK=4096`, `QUEUE=12`).
- Qualif scripts:
  - `bash scripts/test_terminal.sh` -> `PASS`
  - `python3 -m unittest scripts/test_check_web_route_parity.py` -> `PASS`
  - `python3 scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json` -> `PASS`
- Résiduel:
  - endurance 10 min sans assert à confirmer en monitor live.
  - validation E2E entrant GSM -> ring RTC -> `BT_ANSWER` encore `pending`.
- Gouvernance:
  - Issues mises à jour avec preuves:
    - `#10`: `https://github.com/electron-rare/RTC_BL_PHONE/issues/10#issuecomment-3941685856`
    - `#11`: `https://github.com/electron-rare/RTC_BL_PHONE/issues/11#issuecomment-3941685860`
