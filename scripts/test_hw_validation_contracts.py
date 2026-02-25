#!/usr/bin/env python3
"""Unit tests for hw_validation serial smoke contract behavior."""

from __future__ import annotations

import unittest

from scripts.hw_validation import (
    evaluate_serial_smoke_contract,
    scenario_serial_hook_ring_audio,
    scenario_serial_network,
)


def _ack(ok: bool, command: str) -> dict[str, object]:
    return {"ok": ok, "line": ("OK " if ok else "ERR ") + command}


def _make_serial_details(
    *,
    enable_capture: bool,
    capture_start_ok: bool,
    call_ok: bool = True,
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
        "call": _ack(call_ok, "CALL"),
        "capture_start": _ack(capture_start_ok, "CAPTURE_START"),
        "capture_stop": _ack(True, "CAPTURE_STOP"),
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
        details = _make_serial_details(enable_capture=True, capture_start_ok=True, call_ok=False)
        state, _, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=False,
            allow_capture_fail_when_disabled=True,
        )
        self.assertEqual(state, "PASS")
        self.assertIn("CALL", failed_checks)
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


class SerialNetworkContractTest(unittest.TestCase):
    def test_wifi_connect_failure_marks_network_fail(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"connected": False, "ssid": "", "status": 6},
                [{"ssid": "Les cils", "rssi": -42, "chan": 11, "enc": 4}],
                {"ok": False, "line": "ERR WIFI_CONNECT failed"},
                {"connected": False, "ssid": "", "status": 6},
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
                {"ready": True, "peer_count": 1},
            ]
        )
        result = scenario_serial_network(fake, "Les cils", "mascarade")
        self.assertEqual(result.state, "PASS")
        self.assertEqual(result.name, "serial_network_stack")


class SerialHookRingAudioContractTest(unittest.TestCase):
    def test_tone_on_failure_marks_hook_ring_audio_fail(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": False}},
                {"ok": True, "line": "OK RING"},
                {"ok": False, "line": "ERR TONE_ON audio_not_ready"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": False}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": False}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=True, hook_observe_seconds=0)
        self.assertEqual(result.state, "FAIL")
        self.assertEqual(result.name, "serial_hook_ring_audio")
        self.assertEqual(result.details.get("checks", {}).get("tone_on_ok"), False)

    def test_hook_toggle_is_required_when_enabled(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": False}},
                {"ok": True, "line": "OK RING"},
                {"ok": True, "line": "OK TONE_ON"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": True}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": False}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=True, hook_observe_seconds=0)
        self.assertEqual(result.state, "FAIL")
        self.assertEqual(result.details.get("checks", {}).get("hook_ok"), False)

    def test_hook_toggle_passes_when_both_states_are_seen(self) -> None:
        fake = _FakeSerialEndpoint(
            [
                {"telephony": {"hook": "ON_HOOK"}, "audio": {"dial_tone_active": False}},
                {"ok": True, "line": "OK RING"},
                {"ok": True, "line": "OK TONE_ON"},
                {"telephony": {"hook": "OFF_HOOK"}, "audio": {"dial_tone_active": True}},
                {"ok": True, "line": "OK TONE_OFF"},
                {"telephony": {"hook": "OFF_HOOK"}, "audio": {"dial_tone_active": False}},
            ]
        )
        result = scenario_serial_hook_ring_audio(fake, require_hook_toggle=True, hook_observe_seconds=0)
        self.assertEqual(result.state, "PASS")
        self.assertEqual(result.name, "serial_hook_ring_audio")


if __name__ == "__main__":
    unittest.main()
