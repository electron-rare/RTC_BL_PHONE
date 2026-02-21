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
- `scripts/check_web_route_parity.py` (local) : backend routes 29, frontend routes 29, check passé
- `scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json` : OK (report généré)
- `platformio run -e esp32-s3-devkitc-1` : échec connu sur liens Bluetooth/HFP externes (suivi séparé)

## 3-bis) État spec BT HFP/PBAP/Numérotation

- HFP commande-level: OK (connect/disconnect commandes acceptées).
- HFP call-control AT: implémenté (`BT_DIAL`/`DIAL`, `BT_REDIAL`, `BT_ANSWER`, `BT_HANGUP`, `BT_CALLS`).
- HFP liaison réelle: KO sur tests assistés (pas de montée `connected=true`/`hfp_active=true`).
- PBAP: bloqué sur stack actuelle (`arduino-esp32` bluedroid sans API PBAP exposée côté firmware), `BT_PBAP_SYNC` retourne `unsupported`.
- Numérotation HFP/AT: implémentée côté firmware, validation E2E téléphone encore à obtenir (dépend de la liaison HFP réelle).

## 4) Lacunes à combler pour passer la spec en `DONE`

1. Mettre à jour les issues de pilotage (`#10`, `#11`) avec les liens vers les runs CI et l’artefact.
2. Conserver le suivi S3 Bluetooth/HFP hors scope parity V1.

## 5) Risques actifs

- Écart documentation / exécution (doD non uniformément tracée)
- Dépendances CI sur les branches de support non conservées (si nettoyage de branches engagé)
- Blocs matériels/firmware S3 (liens Bluetooth/HFP) pouvant masquer la visibilité produit
