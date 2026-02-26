# Contrat board A252 (source de vérité)

Ce document résume la spec canonique A252.

Source canonique:
- [docs/specs/ai_thinker_esp32_a1s_es8388_n4r8.agent.v2.yaml](./specs/ai_thinker_esp32_a1s_es8388_n4r8.agent.v2.yaml)

## Profil cible
- Carte: Ai-Thinker ESP32-A1S (ES8388), N4R8
- Flash: 4 MB
- PSRAM: 8 MB (4 MB mappés par défaut, HIMEM pour >4 MB)
- Branche: `esp32_RTC_ZACUS`
- Cible unique de validation: A252 (`/dev/cu.usbserial-0001`)

## DIP verrouillé
- DIP1: OFF
- DIP2: ON
- DIP3: ON
- DIP4: OFF
- DIP5: OFF

Effets:
- SD SPI active (CS GPIO13, MOSI GPIO15)
- KEY2 indisponible (partage GPIO13)
- JTAG désactivé

## Mapping pins (normatif)
| Domaine | Signal | GPIO |
|---|---|---|
| I2C ES8388 | SDA | 33 |
| I2C ES8388 | SCL | 32 |
| ES8388 | Adresse 7-bit | 0x10 |
| I2S | MCLK | 0 |
| I2S | BCLK | 27 |
| I2S | LRCK | 25 |
| I2S | DOUT (ESP->codec) | 26 |
| I2S | DIN (codec->ESP) | 35 |
| AMP | ENABLE | 21 (actif HIGH) |
| HP detect | HP_DETECT | 39 (input-only) |
| SLIC | RM | 18 |
| SLIC | FR | 5 |
| SLIC | SHK | 23 |
| SLIC | PD | 19 |
| SD SPI | CS | 13 |
| SD SPI | SCK | 14 |
| SD SPI | MOSI | 15 |
| SD SPI | MISO | 2 |

## Contraintes GPIO
- GPIO34..GPIO39: input-only.
- GPIO34..GPIO39: pas de pull-up/down interne logiciel.
- GPIO0, GPIO2, GPIO15: strapping pins (ne pas perturber les niveaux de boot).
- GPIO13 partagé (SD CS/KEY2/JTAG): dédié SD avec DIP courant.

## Variants supportés
- Variant principal (obligatoire): SDA33/SCL32.
- Fallback I2C seulement si codec non détecté: SDA23/SCL18.

## Compat RTC_BL_PHONE (actuel)
- Source centralisée des pins: `src/config/a1s_board_pins.h`.
- Defaults runtime: `src/config/A252ConfigStore.h`.
- Politique média logique: `LITTLEFS` côté contrat; implémentation `FFat` acceptable si exposée clairement en status/commandes.
- Tonalité A252: référent tonal régional et mappings événement WAV dans [docs/audio_tone_plan.md](./audio_tone_plan.md).
- Variante tonale de référence: `docs/specs/tone_plan_wav_assets/mapping_event_to_wav.yaml` (schema 1.2)

## Checklist rapide de validation
1. `STATUS` expose `config.pins` conforme au tableau.
2. `hw.init_ok`, `hw.slic_ready`, `hw.codec_ready`, `hw.audio_ready` à `true`.
3. `serial_hook_ring_audio` passe (`RING`, `TONE_ON`, `TONE_OFF`, ON/OFF_HOOK vus).
4. I2C codec visible sur `0x10` (ou fallback variant documenté).
