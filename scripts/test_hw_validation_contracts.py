#!/usr/bin/env python3
"""Unit tests for hw_validation serial smoke contract behavior."""

from __future__ import annotations

import unittest

from scripts.hw_validation import (
    evaluate_serial_smoke_contract,
    scenario_serial_audio_format_chain,
    scenario_serial_firmware_contract,
    scenario_serial_hook_ring_audio,
    scenario_serial_media_routing,
    scenario_serial_network,
)


def _ack(ok: bool, command: str) -> dict[str, object]:
    return {"ok": ok, "line": ("OK " if ok else "ERR ") + command}


def _make_serial_details(
    *,
    enable_capture: bool,
    capture_start_ok: bool,
    capture_stop_ok: bool = True,
    ping_ok: bool = True,
) -> dict[str, object]:
    return {
        "ping": {"ok": ping_ok, "result": "PONG" if ping_ok else "ERR"},
        "status": {
            "telephony": {"state": "IDLE"},
            "config": {
                "audio": {
                    "enable_capture": enable_capture,
                }
            },
        },
        "capture_start": _ack(capture_start_ok, "CAPTURE_START"),
        "capture_stop": _ack(capture_stop_ok, "CAPTURE_STOP"),
        "reset_metrics": _ack(True, "RESET_METRICS"),
    }


class SerialSmokeContractTest(unittest.TestCase):
    def test_capture_start_failure_fails_when_capture_enabled(self) -> None:
        details = _make_serial_details(enable_capture=True, capture_start_ok=False)
        state, required_checks, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=True,
            allow_capture_fail_when_disabled=True,
        )
        self.assertEqual(state, "FAIL")
        self.assertIn("CAPTURE_START", required_checks)
        self.assertIn("CAPTURE_START", failed_checks)
        self.assertEqual(warnings, [])

    def test_capture_start_failure_passes_with_warning_when_capture_disabled(self) -> None:
        details = _make_serial_details(enable_capture=False, capture_start_ok=False)
        state, required_checks, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=True,
            allow_capture_fail_when_disabled=True,
        )
        self.assertEqual(state, "PASS")
        self.assertNotIn("CAPTURE_START", required_checks)
        self.assertNotIn("CAPTURE_START", failed_checks)
        self.assertIn("capture_start_failed_capture_disabled", warnings)

    def test_capture_start_failure_can_be_forced_to_fail_when_capture_disabled(self) -> None:
        details = _make_serial_details(enable_capture=False, capture_start_ok=False)
        state, required_checks, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=True,
            allow_capture_fail_when_disabled=False,
        )
        self.assertEqual(state, "FAIL")
        self.assertIn("CAPTURE_START", required_checks)
        self.assertIn("CAPTURE_START", failed_checks)
        self.assertIn("capture_start_required_even_when_capture_disabled", warnings)

    def test_non_strict_mode_warns_but_can_pass_non_critical_failures(self) -> None:
        details = _make_serial_details(enable_capture=True, capture_start_ok=True, capture_stop_ok=False)
        state, _, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=False,
            allow_capture_fail_when_disabled=True,
        )
        self.assertEqual(state, "PASS")
        self.assertIn("CAPTURE_STOP", failed_checks)
        self.assertIn("strict_serial_smoke_disabled", warnings)

    def test_non_strict_mode_still_fails_on_minimum_contract(self) -> None:
        details = _make_serial_details(enable_capture=True, capture_start_ok=True, ping_ok=False)
        state, _, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=False,
            allow_capture_fail_when_disabled=True,
        )
        self.assertEqual(state, "FAIL")
        self.assertIn("PING", failed_checks)
        self.assertNotIn("strict_serial_smoke_disabled", warnings)


class _FakeSerialEndpoint:
    def __init__(self, responses: list[object]) -> None:
        self._responses = responses
        self._index = 0

    def command(self, cmd: str, timeout_s: float = 6.0, expect: str = "any") -> dict[str, object]:
        (timeout_s, expect)  # silence unused
        if self._index >= len(self._responses):
            raise RuntimeError(f"no response configured for command: {cmd}")
        value = self._responses[self._index]
        self._index += 1
        if isinstance(value, Exception):
            raise value
        return value


def _status_audio_contract_payload() -> dict[str, object]:
    return {
        "tone_route_active": False,
        "tone_rendering": False,
        "playback_input_sample_rate": 8000,
        "playback_input_bits_per_sample": 16,
        "playback_input_channels": 1,
        "playback_output_sample_rate": 8000,
        "playback_output_bits_per_sample": 16,
        "playback_output_channels": 2,
        "playback_resampler_active": False,
        "playback_channel_upmix_active": True,
        "playback_loudness_gain_db": 0.0,
        "playback_limiter_active": False,
        "playback_rate_fallback": 0,
    }


class SerialFirmwareContractTest(unittest.TestCase):
    def test_firmware_contract_passes_when_required_blocks_exist(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {
                    "firmware": {
                        "build_id": "Feb 26",
                        "git_sha": "abc123",
                        "contract_version": "A252_AUDIO_CHAIN_V2",
                    },
                    "audio": _status_audio_contract_payload(),
                }
            ]
        )
        result = scenario_serial_firmware_contract(fake, required_contract_version="A252_AUDIO_CHAIN_V2")
        self.assertEqual(result.state, "PASS")
        self.assertEqual(result.name, "serial_firmware_contract")

    def test_firmware_contract_fails_when_version_mismatch(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {
                    "firmware": {
                        "build_id": "Feb 26",
                        "git_sha": "abc123",
                        "contract_version": "OLD_CONTRACT",
                    },
                    "audio": _status_audio_contract_payload(),
                }
            ]
        )
        result = scenario_serial_firmware_contract(fake, required_contract_version="A252_AUDIO_CHAIN_V2")
        self.assertEqual(result.state, "FAIL")
        self.assertFalse(result.details.get("checks", {}).get("contract_version_matches", True))


class SerialNetworkContractTest(unittest.TestCase):
    def test_wifi_connect_failure_marks_network_fail(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"connected": False, "ssid": "", "status": 6},
                [{"ssid": "Les cils", "rssi": -42, "chan": 11, "enc": 4}],
                {"ok": False, "line": "ERR WIFI_CONNECT failed"},
                {"connected": False, "ssid": "", "status": 6},
                {"ok": True, "result": "PONG"},
                {"ready": True, "peer_count": 1},
            ]
        )
        result = scenario_serial_network(fake, "Les cils", "mascarade")
        self.assertEqual(result.state, "FAIL")
        self.assertEqual(result.name, "serial_network_stack")
        self.assertEqual(result.details.get("wifi_connect", {}).get("ok"), False)

    def test_wifi_connect_success_marks_network_pass(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"connected": False, "ssid": "", "status": 6},
                [{"ssid": "Les cils", "rssi": -42, "chan": 11, "enc": 4}],
                {"ok": True, "line": "OK WIFI_CONNECT"},
                {"connected": True, "ssid": "Les cils", "status": 3, "ip": "192.168.1.42"},
                {"ok": True, "result": "PONG"},
                {"ready": True, "peer_count": 1},
            ]
        )
        result = scenario_serial_network(fake, "Les cils", "mascarade")
        self.assertEqual(result.state, "PASS")
        self.assertEqual(result.name, "serial_network_stack")

    def test_wifi_connect_success_but_ssid_mismatch_fails(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"connected": False, "ssid": "", "status": 6},
                [{"ssid": "Les cils", "rssi": -42, "chan": 11, "enc": 4}],
                {"ok": True, "line": "OK WIFI_CONNECT"},
                {"connected": True, "ssid": "OtherSSID", "status": 3, "ip": "192.168.1.42"},
                {"ok": True, "result": "PONG"},
                {"ready": True, "peer_count": 1},
            ]
        )
        result = scenario_serial_network(fake, "Les cils", "mascarade")
        self.assertEqual(result.state, "FAIL")


class SerialHookRingAudioContractTest(unittest.TestCase):
    def test_tone_on_failure_marks_hook_ring_audio_fail(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
                {"ok": False, "line": "ERR TONE_ON audio_not_ready"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=False, hook_observe_seconds=0)
        self.assertEqual(result.state, "FAIL")
        self.assertEqual(result.name, "serial_hook_ring_audio")
        self.assertEqual(result.details.get("checks", {}).get("tone_on_ok"), False)

    def test_hook_toggle_is_required_when_enabled(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
                {"ok": True, "line": "OK TONE_ON"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": True, "tone_event": "dial"}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=True, hook_observe_seconds=0)
        self.assertEqual(result.state, "FAIL")
        self.assertEqual(result.details.get("checks", {}).get("hook_ok"), False)

    def test_hook_toggle_passes_when_both_states_are_seen(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
                {"ok": True, "line": "OK TONE_ON"},
                {"telephony": {"hook": "OFF_HOOK"}, "audio": {"tone_active": True, "tone_event": "dial"}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "OFF_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=True, hook_observe_seconds=0)
        self.assertEqual(result.state, "PASS")
        self.assertEqual(result.name, "serial_hook_ring_audio")

    def test_hook_not_required_passes_when_hooks_not_seen(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
                {"ok": True, "line": "OK TONE_ON"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": True, "tone_event": "dial"}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"tone_active": False, "tone_event": "none"}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=True, hook_observe_seconds=0)
        self.assertEqual(result.state, "PASS")
        self.assertEqual(result.name, "serial_hook_ring_audio")
        self.assertEqual(result.details.get("checks", {}).get("hook_ok"), True)
        self.assertEqual(result.details.get("hook_validation_mode"), "BYPASSED_NON_PRESENTIEL")


class SerialMediaRoutingContractTest(unittest.TestCase):
    def test_media_routing_status_contract_passes(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"ok": True, "line": "OK DIAL_MEDIA_MAP_SET"},
                {"0123456789": {"kind": "tone", "profile": "FR_FR", "event": "ringback"}},
                {"ok": True, "line": "OK ESPNOW_CALL_MAP_SET"},
                {"ok": False, "line": "ERR ESPNOW_CALL_MAP_SET tone_wav_deprecated_use_kind_tone LA_BUSY"},
                {"LA_OK": {"kind": "tone", "profile": "FR_FR", "event": "busy"}},
                {"ok": False, "line": "ERR PLAY tone_wav_deprecated_use_TONE_PLAY"},
                {"ok": True, "line": "OK TONE_PLAY"},
                {
                    "audio": {
                        "storage_default_policy": "SD_THEN_LITTLEFS",
                        "storage_last_source": "NONE",
                        "storage_last_path": "",
                        "tone_route_active": True,
                        "tone_rendering": True,
                        "tone_active": True,
                        "tone_profile": "FR_FR",
                        "tone_event": "busy",
                        "tone_engine": "CODE",
                        "playback_sample_rate": 8000,
                        "playback_bits_per_sample": 16,
                        "playback_channels": 2,
                        "playback_format_overridden": False,
                    },
                    "config": {
                        "audio": {
                            "sample_rate": 8000,
                            "bits_per_sample": 16,
                        },
                        "dial_media_map": {"0123456789": {"kind": "tone", "profile": "FR_FR", "event": "ringback"}},
                        "espnow_call_map": {"LA_OK": {"kind": "tone", "profile": "FR_FR", "event": "busy"}},
                    },
                },
                {"ok": True, "line": "OK TONE_STOP"},
                {
                    "audio": {
                        "tone_route_active": False,
                        "tone_rendering": False,
                        "tone_active": False,
                        "tone_profile": "NONE",
                        "tone_event": "none",
                        "tone_engine": "NONE",
                    }
                },
            ]
        )
        result = scenario_serial_media_routing(fake)
        self.assertEqual(result.state, "PASS")

    def test_media_routing_fails_when_legacy_tone_path_is_not_rejected(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"ok": True, "line": "OK DIAL_MEDIA_MAP_SET"},
                {"0123456789": {"kind": "tone", "profile": "FR_FR", "event": "ringback"}},
                {"ok": True, "line": "OK ESPNOW_CALL_MAP_SET"},
                {"ok": False, "line": "ERR ESPNOW_CALL_MAP_SET tone_wav_deprecated_use_kind_tone LA_BUSY"},
                {"LA_OK": {"kind": "tone", "profile": "FR_FR", "event": "busy"}},
                {"ok": True, "line": "OK PLAY"},
                {
                    "audio": {
                        "storage_default_policy": "SD_THEN_LITTLEFS",
                        "storage_last_source": "LITTLEFS",
                        "storage_last_path": "/assets/wav/FR_FR/dial.wav",
                        "tone_route_active": True,
                        "tone_rendering": True,
                        "tone_active": True,
                        "tone_profile": "FR_FR",
                        "tone_event": "busy",
                        "tone_engine": "CODE",
                        "playback_sample_rate": 8000,
                        "playback_bits_per_sample": 16,
                        "playback_channels": 2,
                        "playback_format_overridden": False,
                    },
                    "config": {
                        "audio": {
                            "sample_rate": 8000,
                            "bits_per_sample": 16,
                        },
                        "dial_media_map": {"0123456789": {"kind": "tone", "profile": "FR_FR", "event": "ringback"}},
                        "espnow_call_map": {"LA_OK": {"kind": "tone", "profile": "FR_FR", "event": "busy"}},
                    },
                },
                {"ok": True, "line": "OK TONE_STOP"},
                {
                    "audio": {
                        "tone_route_active": False,
                        "tone_rendering": False,
                        "tone_active": False,
                        "tone_profile": "NONE",
                        "tone_event": "none",
                        "tone_engine": "NONE",
                    }
                },
            ]
        )
        result = scenario_serial_media_routing(fake)
        self.assertEqual(result.state, "FAIL")


class SerialAudioFormatChainContractTest(unittest.TestCase):
    def test_audio_format_chain_passes_with_probe_and_status_alignment(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {
                    "clock_policy": "HYBRID_TELCO",
                    "wav_loudness_policy": "AUTO_NORMALIZE_LIMITER",
                    "wav_target_rms_dbfs": -18,
                    "wav_limiter_ceiling_dbfs": -2,
                    "wav_limiter_attack_ms": 8,
                    "wav_limiter_release_ms": 120,
                },
                {
                    "ok": True,
                    "path": "/welcome.wav",
                    "source": "LITTLEFS",
                    "input_sample_rate": 16000,
                    "input_bits_per_sample": 16,
                    "input_channels": 1,
                    "output_sample_rate": 16000,
                    "output_bits_per_sample": 16,
                    "output_channels": 2,
                    "resampler_active": False,
                    "channel_upmix_active": True,
                    "loudness_auto": True,
                    "loudness_gain_db": 2.5,
                    "limiter_active": False,
                    "rate_fallback": 0,
                },
                {"ok": True, "line": "OK PLAY"},
                {
                    "audio": {
                        "tone_route_active": False,
                        "tone_rendering": False,
                        "playback_input_sample_rate": 16000,
                        "playback_input_bits_per_sample": 16,
                        "playback_input_channels": 1,
                        "playback_output_sample_rate": 16000,
                        "playback_output_bits_per_sample": 16,
                        "playback_output_channels": 2,
                        "playback_resampler_active": False,
                        "playback_channel_upmix_active": True,
                        "playback_loudness_gain_db": 2.5,
                        "playback_limiter_active": False,
                        "playback_rate_fallback": 0,
                    }
                },
            ]
        )
        result = scenario_serial_audio_format_chain(fake, "/welcome.wav")
        self.assertEqual(result.state, "PASS")
        self.assertTrue(result.details.get("checks", {}).get("status_matches_probe_rate"))

    def test_audio_format_chain_fails_when_probe_not_available(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {
                    "clock_policy": "HYBRID_TELCO",
                    "wav_loudness_policy": "AUTO_NORMALIZE_LIMITER",
                },
                {"ok": False, "line": "ERR AUDIO_PROBE file_not_found"},
                {"ok": False, "line": "ERR AUDIO_PROBE file_not_found"},
            ]
        )
        result = scenario_serial_audio_format_chain(fake, "/welcome.wav")
        self.assertEqual(result.state, "FAIL")
        self.assertFalse(result.details.get("checks", {}).get("audio_probe_found_supported_file", True))


if __name__ == "__main__":
    unittest.main()
