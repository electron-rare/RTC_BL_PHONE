# A252 Strict Gate Summary

- Date UTC: 2026-02-25T15:17:05.494914+00:00
- Verdict global: PASS
- Port A252: `/dev/cu.usbserial-0001`
- ZeroClaw: `http://127.0.0.1:8788`

## Étapes

| Étape | État |
|---|---|
| zeroclaw_hw_preflight | PASS |
| zeroclaw_orchestrator_health | PASS |
| branch_gate_profile_a252 | PASS |
| hw_validation_a252 | PASS |

## Stacks A252 (hw_validation)

| Stack/Scénario | État |
|---|---|
| serial_smoke | PASS |
| serial_hook_ring_audio | PASS |
| serial_media_routing | PASS |
| serial_network_stack | PASS |
| http_endpoints | PASS |
| manual_hook_transition | MANUAL_SKIP |
| manual_ring_behavior | MANUAL_SKIP |
| manual_audio_path | MANUAL_SKIP |
| manual_hfp_pairing | MANUAL_SKIP |

## ZeroClaw Docker

| Check | État |
|---|---|
| status | PASS |
| agents | PASS |
| workflows | PASS |
| provider_scan | PASS |
