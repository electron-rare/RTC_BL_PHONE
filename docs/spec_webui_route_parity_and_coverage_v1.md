# Spécification Orchestrateur RTC — WebUI Route Parity & Coverage (v1)

Date: 2026-02-20  
Repo: `electron-rare/RTC_BL_PHONE`  
Issues liées: `#10`, `#11`

## 1. Contexte

Le backend HTTP firmware expose un ensemble de routes `/api/...` plus large que celles utilisées par la WebUI.  
Des régressions passées ont montré que la dérive frontend/backend doit être contrôlée par un gate dédié.

Références d'exécution:
- PR #7 (alignement routes WebUI/backend)
- PR #9 (hardening + validation hardware)
- PR #15 (correctif parser parity)

## 2. Objectif

Mettre en place une spécification unique qui couvre:
1. la parité stricte des routes appelées par la WebUI,
2. l'extension de la WebUI vers les routes backend encore non exposées.

## 3. Périmètre

In-scope:
- route parity CI entre:
  - frontend: `data/webui/script.js`
  - backend: `src/web/WebServerManager.cpp`
- couverture UI des endpoints backend existants non encore pilotés.

Out-of-scope:
- redesign visuel complet de la WebUI,
- modification du contrat ESP-NOW inter-repos.

## 4. Exigences fonctionnelles

### EF-01 — Gate route parity
- Le script `scripts/check_web_route_parity.py` DOIT être exécuté en CI.
- Le gate DOIT échouer si une route appelée en WebUI n'existe pas côté backend.

### EF-02 — Artefact de preuve parity
- Chaque run de gate DOIT produire un artefact lisible contenant:
  - routes frontend détectées,
  - routes backend détectées,
  - delta (missing frontend->backend, backend non utilisés).

### EF-03 — Couverture WebUI des routes backend
- La WebUI DOIT couvrir les routes de pilotage/lecture non encore exposées côté opérateur:
  - `/api/bluetooth`
  - `/api/bluetooth/hfp/connect`
  - `/api/bluetooth/hfp/disconnect`
  - `/api/bluetooth/ble/start`
  - `/api/bluetooth/ble/stop`
  - `/api/config/audio`
  - `/api/config/mqtt`
  - `/api/config/pins`
  - `/api/network/mqtt`
  - `/api/network/espnow`
  - `/api/network/espnow/peer`

### EF-04 — Non-régression des routes existantes
- Les flux déjà supportés DOIVENT rester fonctionnels:
  - status global,
  - wifi connect/disconnect/reconnect,
  - mqtt connect/disconnect/publish,
  - espnow on/off/send/peer,
  - control actions.

### Preuve d’état (mise à jour)

- Local (2026-02-21) : `scripts/check_web_route_parity.py` -> `backend routes: 29 | frontend routes: 29` et `parity check passed`.
- CI : le check parity s’exécute via `.github/workflows/ci.yml` avec export (`python3 scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json`).
- EF-02 : artefact dédié `route-parity-report` publié via `actions/upload-artifact`.

## 5. Exigences non fonctionnelles

- Exécution gate parity < 3 secondes en CI standard.
- Zéro dépendance réseau externe pour le gate parity.
- Messages d'échec actionnables (route + méthode).

## 6. Plan de livraison

Phase A (gate):
1. stabiliser la sortie du checker parity,
2. l'intégrer à `ci.yml`,
3. publier un artefact parity.

Phase B (coverage UI):
1. ajouter les blocs UI manquants,
2. connecter les endpoints listés en EF-03,
3. enrichir le rapport fonctionnel.

## 7. Critères d'acceptation (DoD)

- [x] `scripts/check_web_route_parity.py` exécuté en CI.
- [x] Échec CI confirmé quand une route frontend orpheline est injectée (test négatif).
- [x] Les routes EF-03 sont accessibles depuis la WebUI.
- [x] `docs/rapport_tests_fonctionnels.md` inclut la vérification de ces routes.
- [ ] Issue `#11` mise à jour avec run CI de preuve.
- [ ] Issue `#10` mise à jour avec captures/JSON de couverture.

## 8. Coordination inter-repos

- Companion Zacus: issue `le-mystere-professeur-zacus#94`.
- Contrat ESP-NOW inchangé: `cmd/raw/command/action` + `event/message/payload`.
