# Gate de branche (pré-validation)

Ce document décrit l’ordonnancement unique des contrôles de conformité avant transfert.

## Ordre d’exécution

Le script `scripts/branch_gate.sh` exécute les contrôles suivants, dans l’ordre :

1. `python3 -m py_compile scripts/hw_validation.py`
2. `platformio test --without-uploading --without-testing -e esp32dev`
3. Compilation + exécution du test hôte DTMF:
   - `c++ -std=c++17 -Wall -Wextra -pedantic -Isrc test/host/test_dtmf_host.cpp src/telephony/DtmfDecoder.cpp -o .pio/host/test_dtmf_host`
   - `.pio/host/test_dtmf_host`
4. `python3 -m unittest scripts/test_check_web_route_parity.py scripts/test_runtime_contracts.py scripts/test_hw_validation_contracts.py`
5. `python3 scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json`
6. Build PlatformIO des cibles selon le profil :
   - Profil `a252` (défaut): `esp32dev`
   - Profil `full`: `esp32dev`, `esp32-s3-devkitc-1`, `esp32-s3-usb-host`, `esp32-s3-usb-msc`

## Contrat de cette branche

- **ESP-NOW obligatoire** : seule la pile de transport activée est prise en compte pour les scénarios de validation.
- **Bluetooth retiré** : aucun endpoint/commande Bluetooth n’est attendu, ni traité.
- **Auth web Wi-Fi désactivée** : la validation considère les endpoints web accessibles sans authentification basique.

## Cible CI

- `.github/workflows/ci.yml` lance `scripts/branch_gate.sh --profile a252`.
- L’artefact `artifacts/route_parity_report.json` est uploadé automatiquement.

## Options utiles

- `--skip-builds` : ignore uniquement la phase build.
- `--profile <a252|full>` : sélectionne le profil de build par défaut.
- `--build-env <env>` : ajoute un env PlatformIO spécifique.
- `--build-envs <env1,env2>` : ajoute plusieurs envs.
- `--report-json <path>` : redirige le rapport parity JSON.

Exemples :

```bash
bash scripts/branch_gate.sh --skip-builds
bash scripts/branch_gate.sh --profile a252
bash scripts/branch_gate.sh --profile full
bash scripts/branch_gate.sh --build-env esp32dev --build-env esp32-s3-devkitc-1
```

## Profil standard

- Le profil standard de cette branche est `a252` (A252 strict).
- Le profil `full` reste disponible pour les campagnes de compatibilité multi-cartes.

## Redirection historique

- `docs/pre_merge_checks.md` est conservé comme redirection vers ce document.
