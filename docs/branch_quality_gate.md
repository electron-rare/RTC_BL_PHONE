# Gate de branche (pré-validation)

Ce document décrit l’ordonnancement unique des contrôles de conformité avant transfert.

## Ordre d’exécution

Le script `scripts/pre_merge.sh` exécute les contrôles suivants, dans l’ordre :

1. `python3 -m py_compile scripts/hw_validation.py`
2. `platformio test --without-uploading --without-testing -e esp32dev`
3. Compilation + exécution du test hôte DTMF:
   - `c++ -std=c++17 -Wall -Wextra -pedantic -Isrc test/host/test_dtmf_host.cpp src/telephony/DtmfDecoder.cpp -o .pio/host/test_dtmf_host`
   - `.pio/host/test_dtmf_host`
4. `python3 -m unittest scripts/test_check_web_route_parity.py scripts/test_runtime_contracts.py`
5. `python3 scripts/check_web_route_parity.py --report-json artifacts/route_parity_report.json`
6. Build PlatformIO des cibles (par défaut) :
   - `esp32dev`
   - `esp32-s3-devkitc-1`
   - `esp32-s3-usb-host`
   - `esp32-s3-usb-msc`

## Cible CI

- `.github/workflows/ci.yml` lance directement `scripts/pre_merge.sh`.
- L’artefact `artifacts/route_parity_report.json` est uploadé automatiquement.

## Options utiles

- `--skip-builds` : ignore uniquement la phase build.
- `--build-env <env>` : ajoute un env PlatformIO spécifique.
- `--build-envs <env1,env2>` : ajoute plusieurs envs.
- `--report-json <path>` : redirige le rapport parity JSON.

Exemples :

```bash
bash scripts/pre_merge.sh --skip-builds
bash scripts/pre_merge.sh --build-env esp32dev --build-env esp32-s3-devkitc-1
```
