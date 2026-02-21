# Rapport validation HW

- Date UTC: 2026-02-21T18:20:29.000908+00:00
- Verdict global: FAIL

| Scénario | Verdict | Détails |
|---|---|---|
| SLIC transition A252 | FAIL | `{"error": "Timeout waiting response to 'CALL' on /dev/cu.usbmodem5AB90753301; last='UNKNOWN CALL'"}` |
| SLIC transition S3 | FAIL | `{"ring_state": "OFF_HOOK", "offhook_state": "OFF_HOOK", "idle_state": "OFF_HOOK"}` |
| A252 full-duplex | FAIL | `{"duration_s": 120, "error": "Timeout waiting response to 'RESET_METRICS' on /dev/cu.usbmodem5AB90753301; last='UNKNOWN RESET_METRICS'"}` |
| S3 local mode | FAIL | `{"duration_s": 20, "telephony_state": "OFF_HOOK", "audio_drop_frames": 0, "bench_latency_ms": 9999}` |
| A252 web endpoints | PASS | `{"skipped": true}` |
| S3 web endpoints | PASS | `{"skipped": true}` |
