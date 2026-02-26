# A252 Hardware Spec Skill (ESP32-A1S ES8388, N4R8)

Use this skill when working on RTC_BL_PHONE hardware bring-up or diagnostics for the A252 board with:

- SoC/module: ESP32-A1S
- Codec: ES8388
- Memory variant: N4R8 (4MB Flash, 8MB PSRAM)
- DIP: `1 OFF / 2 ON / 3 ON / 4 OFF / 5 OFF`

## Scope Lock

- Target board only: `ESP32_A252` (`esp32dev` env in this repo).
- Do not switch to multi-board runs unless explicitly requested.
- Frontend remains out of scope for this skill.

## Hardware Facts (Locked)

### Memory

- Flash: 4MB (`N4`)
- PSRAM: 8MB physical (`R8`)
- Note: default mapped PSRAM is typically limited to 4MB; >4MB requires HIMEM APIs.

### DIP Semantics

- DIP1 ON: `GPIO13 <-> KEY2`
- DIP2 ON: `GPIO13 <-> SD_DATA3` (SD CS in SPI mode)
- DIP3 ON: `GPIO15 <-> SD_CMD` (SD MOSI in SPI mode)
- DIP4 ON: `GPIO13 <-> JTAG MTCK`
- DIP5 ON: `GPIO15 <-> JTAG MTDO`

With locked DIP profile (`1 OFF / 2 ON / 3 ON / 4 OFF / 5 OFF`):

- SD over SPI active on shared lines (`GPIO13`, `GPIO15`)
- KEY2 unavailable
- JTAG lines disconnected

## Canonical Pin Mapping (A252 / ES8388)

### I2C (ES8388 control)

- `SCL = GPIO32`
- `SDA = GPIO33`
- `ES8388 addr = 0x10` (7-bit)

### I2S (audio data)

- `MCLK = GPIO0`
- `BCLK = GPIO27`
- `LRCK/WS = GPIO25`
- `DOUT (ESP->codec) = GPIO26`
- `DIN (codec->ESP) = GPIO35`

### Amp + HP detect

- `PA_ENABLE = GPIO21` (active HIGH)
- `HP_DETECT = GPIO39` (input-only; typical active LOW)

### SD (SPI)

- `CS = GPIO13`
- `MISO = GPIO2`
- `MOSI = GPIO15`
- `SCK = GPIO14`

### Audio tone contract

- Runtime tonalité A252: 100% synthèse code (`TONE_PLAY`/`TONE_STOP`), pas de WAV pour les tones.
- Profils/events canoniques: `FR_FR`, `ETSI_EU`, `UK_GB`, `NA_US` + catalogue tone code.
- Contrat map tone: `{"kind":"tone","profile":"FR_FR","event":"busy"}`.
- Les WAV restent autorisés uniquement pour médias non-tone (`PLAY` file media).
- Référence de design (cadences/fréquences): `docs/specs/tone_plan_wav_assets/tone_plan.yaml`.

### Keys

- `KEY1=36`, `KEY2=13` (not usable with current DIP), `KEY3=19`, `KEY4=23`, `KEY5=18`, `KEY6=5`

## In-Repo Source of Truth

- Header: `src/config/a1s_board_pins.h`
- Runtime status: `STATUS` command and `/api/status` payload
- Gate: `scripts/a252_strict_gate.sh`
- Audio tone contract: `docs/audio_tone_plan.md` + `src/audio/ToneCatalog.*`
- Tone references only: `docs/specs/tone_plan_wav_assets/*`

## Bring-Up Checks (Minimum)

1. I2C scan on `SDA33/SCL32` must detect `0x10`.
2. `GPIO21` toggling must control speaker amp.
3. `GPIO39` must reflect jack insertion state.
4. I2S playback must work with mapping `0/27/25/26/35`.
5. SD SPI requires DIP2+3 ON.
6. Tones A252 must be testable via code API: `TONE_PLAY FR_FR dial` then `TONE_STOP`.
7. Verify `STATUS.audio.tone_route_active`, `STATUS.audio.tone_rendering`, `STATUS.audio.tone_active`, `STATUS.audio.tone_profile`, `STATUS.audio.tone_event`, `STATUS.audio.tone_engine`.
8. Verify `STATUS.firmware.build_id/git_sha/contract_version` and playback chain fields (`playback_input_*`, `playback_output_*`, `playback_resampler_active`, `playback_channel_upmix_active`, `playback_loudness_gain_db`, `playback_limiter_active`, `playback_rate_fallback`).

## Execution Pattern

1. Build: `pio run -e esp32dev`
2. Flash: `pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001`
3. Smoke serial:
   - `PING`
   - `STATUS`
   - `TONE_ON` / `TONE_OFF`
   - `TONE_PLAY FR_FR busy` / `TONE_STOP`
4. Full gate: `bash scripts/a252_strict_gate.sh`
5. Audio chain gate: `python3 scripts/hw_validation.py --port-a252 /dev/cu.usbserial-0001 --no-require-hook-toggle --audio-probe-path /welcome.wav`

## Constraints

- Do not re-enable KEY2 with current SD setup (shared GPIO13).
- Avoid assumptions of software pull-ups on GPIO39.
- Avoid changing board profile defaults unless requested.
