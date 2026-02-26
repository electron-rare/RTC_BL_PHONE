# Tone Plan Assets (YAML/JSON) — multi-region

## Files
- tone_plan.yaml / tone_plan.json
- mapping_event_to_wav.yaml / mapping_event_to_wav.json
- assets/wav_clean_8k/<PROFILE>/<EVENT>.wav
- assets/wav_clean_44k1/<PROFILE>/<EVENT>.wav
- runtime référence: `data/audio/assets/wav/<PROFILE>/<EVENT>.wav`

## WAV format
PCM 16-bit little-endian, mono, 8000 Hz.

## Variants
- `clean_8k` (8000 Hz) : référence courte pour validation locale.
- `clean_44k1` (44100 Hz) : variante de haute fidélité de référence.

## Looping
Use mapping.loop:
- enabled=true => loop samples [start_sample, end_sample)
- then_continuous tones include bursts + appended steady segment; loop starts at steady segment.
