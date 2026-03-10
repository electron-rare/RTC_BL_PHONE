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
- **Routing média A252** : lecture audio supporte `SD`/`LITTLEFS` avec fallback `SD_THEN_LITTLEFS` et mapping numérotation/ESP-NOW persistant.
- **Tone plan tonal** : runtime 100% code (`TONE_PLAY` / `TONE_STOP`, `kind=tone`), WAV conservés uniquement comme référence documentaire dans [docs/audio_tone_plan.md](audio_tone_plan.md) et [docs/specs/tone_plan_wav_assets](specs/tone_plan_wav_assets).
- **Statut tonal/audio** : `STATUS.audio` doit exposer `tone_route_active`, `tone_rendering`, `tone_profile`, `tone_event`, `tone_engine`, et les champs playback complets:
  - `playback_input_sample_rate/bits_per_sample/channels`
  - `playback_output_sample_rate/bits_per_sample/channels`
  - `playback_resampler_active`, `playback_channel_upmix_active`
  - `playback_loudness_auto`, `playback_loudness_gain_db`, `playback_limiter_active`, `playback_rate_fallback`
  - `playback_copy_source_bytes`, `playback_copy_accepted_bytes`, `playback_copy_loss_bytes`, `playback_copy_loss_events`, `playback_last_error`
- **Fingerprint firmware** : `STATUS.firmware` doit exposer `build_id`, `git_sha`, `contract_version` (contrat attendu: `A252_AUDIO_CHAIN_V4`).
- **Board contract A252** : référence matérielle canonique dans [docs/a252_board_spec.md](./a252_board_spec.md) et [docs/specs/ai_thinker_esp32_a1s_es8388_n4r8.agent.v2.yaml](./specs/ai_thinker_esp32_a1s_es8388_n4r8.agent.v2.yaml).

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

## Gate hardware A252 (local)

Pour la validation hardware locale, utiliser `scripts/a252_strict_gate.sh`.

- Mode unattended (défaut): `A252_IGNORE_ZEROCLAW=1` et `A252_REQUIRE_HOOK_TOGGLE=0`.
- Mode strict opérateur: `A252_REQUIRE_HOOK_TOGGLE=1` pour exiger `ON_HOOK` + `OFF_HOOK`.
- `hw_validation.py` exécute les scénarios bloquants `serial_firmware_contract` et `serial_audio_format_chain` pour éviter les diagnostics sur firmware périmé et valider la chaîne format/bitrate.

Exemple:

```bash
A252_WIFI_SSID="Les cils" \
A252_WIFI_PASSWORD="***" \
A252_REQUIRE_HOOK_TOGGLE=1 \
bash scripts/a252_strict_gate.sh
```

## Redirection historique

- `docs/pre_merge_checks.md` est conservé comme redirection vers ce document.
- `docs/audio_tone_plan.md` centralise les spécifications tonales et les assets de tonalité.
