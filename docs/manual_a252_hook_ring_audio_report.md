# Rapport manuel hook/ring/audio (A252)

- Date UTC: 2026-02-25T12:08:20.180373+00:00
- Port: /dev/cu.usbserial-0001
- Observation hook (s): 30

- manual_hook_transition: MANUAL_SKIP
- manual_ring_behavior: PASS
- manual_audio_path: FAIL
- hook_values_observed: ON_HOOK

## Commandes
```json
{
  "status_before": {
    "board_profile": "ESP32_A252",
    "active_scene": "",
    "telephony": {
      "state": "IDLE",
      "hook": "ON_HOOK",
      "pending_espnow_call": false,
      "pending_espnow_call_audio": ""
    },
    "audio_frames_requested": 0,
    "audio_frames_read": 0,
    "audio_drop_frames": 0,
    "audio_underrun_count": 0,
    "audio_last_latency_ms": 0,
    "audio_max_latency_ms": 0,
    "audio": {
      "full_duplex": true,
      "dial_tone_active": false,
      "playing": false,
      "sd_ready": false,
      "frames": 0,
      "underrun": 0,
      "drop": 0,
      "latence_ms": 0,
      "adc_fft_peak_bin": 0,
      "adc_fft_peak_freq_hz": 0,
      "adc_fft_peak_mag": 0
    },
    "scope_display": {
      "supported": false,
      "enabled": false,
      "frequency": 1200,
      "amplitude": 48
    },
    "espnow": {
      "ready": true,
      "peer_count": 1,
      "tx_ok": 0,
      "tx_fail": 0,
      "rx_count": 0,
      "last_rx_mac": "",
      "last_rx_payload": "",
      "peers": [
        "10:20:BA:58:C7:48"
      ]
    },
    "config": {
      "pins": {
        "i2s": {
          "bck": 27,
          "ws": 25,
          "dout": 26,
          "din": 35
        },
        "codec_i2c": {
          "sda": 33,
          "scl": 32
        },
        "slic": {
          "rm": 18,
          "fr": 5,
          "shk": 23,
          "line": -1,
          "pd": 19,
          "adc_in": -1,
          "hook_active_high": true
        },
        "pcm": {
          "flt": -1,
          "demp": -1,
          "xsmt": -1,
          "fmt": -1
        }
      },
      "audio": {
        "sample_rate": 16000,
        "bits_per_sample": 16,
        "enable_capture": false,
        "adc_dsp_enabled": true,
        "adc_fft_enabled": true,
        "adc_dsp_fft_downsample": 2,
        "adc_fft_ignore_low_bin": 1,
        "adc_fft_ignore_high_bin": 1,
        "volume": 100,
        "mute": false,
        "route": "rtc"
      },
      "espnow_call_map": {},
      "espnow_peers": [
        "10:20:BA:58:C7:48"
      ]
    }
  },
  "ring": {
    "ok": true,
    "line": "OK RING"
  },
  "tone_on": {
    "ok": false,
    "line": "ERR TONE_ON"
  },
  "tone_off": {
    "ok": true,
    "line": "OK TONE_OFF"
  },
  "call": {
    "ok": true,
    "line": "OK CALL"
  },
  "status_after": {
    "board_profile": "ESP32_A252",
    "active_scene": "",
    "telephony": {
      "state": "IDLE",
      "hook": "ON_HOOK",
      "pending_espnow_call": false,
      "pending_espnow_call_audio": ""
    },
    "audio_frames_requested": 0,
    "audio_frames_read": 0,
    "audio_drop_frames": 0,
    "audio_underrun_count": 0,
    "audio_last_latency_ms": 0,
    "audio_max_latency_ms": 0,
    "audio": {
      "full_duplex": true,
      "dial_tone_active": false,
      "playing": false,
      "sd_ready": false,
      "frames": 0,
      "underrun": 0,
      "drop": 0,
      "latence_ms": 0,
      "adc_fft_peak_bin": 0,
      "adc_fft_peak_freq_hz": 0,
      "adc_fft_peak_mag": 0
    },
    "scope_display": {
      "supported": false,
      "enabled": false,
      "frequency": 1200,
      "amplitude": 48
    },
    "espnow": {
      "ready": true,
      "peer_count": 1,
      "tx_ok": 0,
      "tx_fail": 0,
      "rx_count": 0,
      "last_rx_mac": "",
      "last_rx_payload": "",
      "peers": [
        "10:20:BA:58:C7:48"
      ]
    },
    "config": {
      "pins": {
        "i2s": {
          "bck": 27,
          "ws": 25,
          "dout": 26,
          "din": 35
        },
        "codec_i2c": {
          "sda": 33,
          "scl": 32
        },
        "slic": {
          "rm": 18,
          "fr": 5,
          "shk": 23,
          "line": -1,
          "pd": 19,
          "adc_in": -1,
          "hook_active_high": true
        },
        "pcm": {
          "flt": -1,
          "demp": -1,
          "xsmt": -1,
          "fmt": -1
        }
      },
      "audio": {
        "sample_rate": 16000,
        "bits_per_sample": 16,
        "enable_capture": false,
        "adc_dsp_enabled": true,
        "adc_fft_enabled": true,
        "adc_dsp_fft_downsample": 2,
        "adc_fft_ignore_low_bin": 1,
        "adc_fft_ignore_high_bin": 1,
        "volume": 100,
        "mute": false,
        "route": "rtc"
      },
      "espnow_call_map": {},
      "espnow_peers": [
        "10:20:BA:58:C7:48"
      ]
    }
  }
}
```
