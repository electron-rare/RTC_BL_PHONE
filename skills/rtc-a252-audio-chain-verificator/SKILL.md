---
name: rtc-a252-audio-chain-verificator
description: Vérifie la chaîne audio A252 de bout en bout (WAV messages + tones code) avec diagnostic timing/bitrate, statut firmware contract et verdict PASS/FAIL matériel.
---

# RTC A252 Audio Chain Verificator

## Quand utiliser
- Rendu audio non conforme (volume faible, pitch faux, artefacts, jitter, drop).
- Suspicion de mismatch format/bitrate (WAV 22.05k/24-bit/mono, etc.).
- Doute sur firmware flashée vs contrat runtime attendu.

## Scope
- Cible unique: `ESP32_A252` (`esp32dev`) sur `/dev/cu.usbserial-0001`.
- Tones téléphonie: génération code (`TONE_PLAY`/`TONE_STOP`).
- WAV: playback fichier adaptatif (SD/LittleFS) avec conversion interne.
- Ne pas modifier/supprimer `data/*`.

## Commandes série de probe
1. `STATUS`
2. `AUDIO_POLICY_GET`
3. `AUDIO_PROBE /welcome.wav` (fallback possible `/musique.wav`)
4. `PLAY /welcome.wav`
5. `STATUS` (poll court jusqu’à observation playback active)
6. `TONE_PLAY FR_FR dial`
7. `TONE_STOP`

## Contrat runtime minimum
- `STATUS.firmware`:
  - `build_id`, `git_sha`, `contract_version`
- `STATUS.audio`:
  - `tone_route_active`, `tone_rendering`, `tone_profile`, `tone_event`, `tone_engine`
  - `playback_input_sample_rate/bits_per_sample/channels`
  - `playback_output_sample_rate/bits_per_sample/channels`
  - `playback_resampler_active`, `playback_channel_upmix_active`
  - `playback_loudness_auto`, `playback_loudness_gain_db`, `playback_limiter_active`, `playback_rate_fallback`
  - `playback_copy_source_bytes`, `playback_copy_accepted_bytes`, `playback_copy_loss_bytes`, `playback_copy_loss_events`, `playback_last_error`

## Matrice formats WAV attendue
- Entrée supportée: bits `{8,16,24,32}`, channels `{1,2}`, rates `8k..48k`.
- Sortie codec attendue: `16-bit`, `1|2 channels` (A252 nominal: stéréo).
- Rates stables cible: `{8000,16000,22050,32000,44100,48000}`.
- Si fallback rate: `playback_rate_fallback > 0` et traçable dans `STATUS.audio`.

## Critères PASS/FAIL matériels
PASS:
- `serial_firmware_contract` PASS.
- `serial_audio_format_chain` PASS.
- `TONE_PLAY` active `tone_route_active=true` puis `TONE_STOP` coupe la route immédiatement.
- WAV probe + PLAY cohérents (input/output + flags resampler/upmix/loudness/limiter).
- Fenêtre playback observée après `PLAY` (`status_playback_window_observed=true`).
- `playback_copy_loss_events == 0` et `playback_copy_loss_bytes == 0`.

FAIL:
- Absence de `STATUS.firmware.contract_version`.
- `PLAY` retourne un format incohérent avec `AUDIO_PROBE`.
- Output bits != 16 sur A252.
- Tones code non détectés dans le statut (`tone_route_active/tone_rendering`).
- Playback non observé après `PLAY`.
- Perte copy détectée (`playback_copy_loss_events > 0` ou `playback_copy_loss_bytes > 0`).

## Workflow recommandé
1. `python3 -m pytest -q scripts/test_hw_validation_contracts.py`
2. `bash scripts/branch_gate.sh --profile a252`
3. `python3 scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001 --no-require-hook-toggle --strict-serial-smoke --allow-capture-fail-when-disabled --audio-probe-path /welcome.wav --require-contract-version A252_AUDIO_CHAIN_V4`
4. Corriger en priorité: contrat firmware -> format chain -> timing tone.
