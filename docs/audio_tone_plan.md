# Tone Plan A252 (Code Runtime)

Ce document définit le contrat runtime des tonalités téléphoniques A252.

## Règle runtime

- Les tonalités telco sont générées en code (`ToneCatalog` + `AudioEngine`).
- `PLAY` est réservé aux médias fichier non-tone.
- Les routes tone legacy en WAV (`/assets/wav/<PROFILE>/<event>.wav`) sont rejetées.

## Profil par défaut

- Profil tonal par défaut: `FR_FR`.

## API série

- `TONE_PLAY <event>`: joue un tone du profil `FR_FR`.
- `TONE_PLAY <profile> <event>`: joue un tone explicite (ex: `TONE_PLAY ETSI_EU busy`).
- `TONE_STOP`: stoppe le tone actif.
- `AUDIO_POLICY_GET`: lit la politique clock/loudness WAV active.
- `AUDIO_POLICY_SET <json>`: met à jour la politique (`clock_policy`, `wav_loudness_policy`, RMS/limiter).
- `AUDIO_PROBE <path>`: résout le format réel d’un WAV (input/output, resampler/upmix, gain/limiter, fallback rate) sans ambiguïté.
- Compatibilité:
  - `TONE_ON` => alias `TONE_PLAY FR_FR dial`
  - `TONE_OFF` => alias `TONE_STOP`

## Routage map

Routes tone explicites:

```json
{
  "LA_BUSY": { "kind": "tone", "profile": "FR_FR", "event": "busy" },
  "0123456789": { "kind": "tone", "profile": "FR_FR", "event": "ringback" }
}
```

Routes fichier non-tone:

- String legacy: `"/media/welcome.wav"` (équivalent `kind=file`, `source=auto`)
- Objet:

```json
{
  "kind": "file",
  "path": "/media/welcome.wav",
  "source": "auto"
}
```

## Statut runtime

`STATUS.audio` expose:

- `tone_active`
- `tone_route_active` (`true` tant que route tone demandée; passe faux dès `TONE_STOP`)
- `tone_rendering` (`true` tant que l'oscillateur sort encore dans la queue de tail)
- `tone_profile`
- `tone_event`
- `tone_engine` (`CODE|NONE`)
- `dial_tone_active` (compatibilité)
- `playback_input_sample_rate`
- `playback_input_bits_per_sample`
- `playback_input_channels`
- `playback_output_sample_rate`
- `playback_output_bits_per_sample`
- `playback_output_channels`
- `playback_resampler_active`
- `playback_channel_upmix_active`
- `playback_loudness_gain_db`
- `playback_limiter_active`
- `playback_rate_fallback`
- `playback_format_overridden`
- `firmware.build_id` / `firmware.git_sha` / `firmware.contract_version`

`tone_route_active` devient faux immédiatement à l'arrêt, même si `tone_rendering` reste vrai quelques centaines de ms pour le tail anti-pop.

## Politique bitrate/format A252

- Sortie codec ES8388: I2S 16-bit forcé.
- Tones code: base 8 kHz telco, cadences déterministes.
- WAV messages: entrée 8/16/24/32-bit, 8 kHz..48 kHz; conversion interne codec-safe avec:
  - resample vers taux stable (`8000/16000/22050/32000/44100/48000`) si nécessaire,
  - upmix mono->stéréo,
  - auto-normalize + limiter léger selon `AUDIO_POLICY_*`.

## Référence de conception

Les cadences/fréquences restent décrites dans:

- `docs/specs/tone_plan_wav_assets/tone_plan.yaml`
- `docs/specs/tone_plan_wav_assets/tone_plan.json`

Les assets WAV sont conservés comme archives/référence documentaire, pas comme moteur runtime tone.
