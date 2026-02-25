#!/usr/bin/env python3
"""A252-only hardware validation runner (without bench controller)."""

from __future__ import annotations

import argparse
import glob
import json
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional
from urllib import error, parse, request

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    serial = None


VALID_STATES = {"PASS", "FAIL", "MANUAL_PASS", "MANUAL_FAIL", "MANUAL_SKIP"}
OPTIONAL_SERIAL_COMMANDS = {"WIFI_SCAN", "ESPNOW_STATUS"}


@dataclass
class ScenarioResult:
    name: str
    state: str
    details: Dict[str, Any]


def run_cmd(cmd: List[str]) -> None:
    print(f"[hw_validation] $ {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


class SerialEndpoint:
    def __init__(self, port: str, baud: int, timeout_s: float = 0.5) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required (pip install pyserial)")
        self.port = port
        self.baud = baud
        self.timeout_s = timeout_s
        self._ser: Optional[serial.Serial] = None

    def __enter__(self) -> "SerialEndpoint":
        self._ser = serial.Serial(self.port, self.baud, timeout=self.timeout_s)
        time.sleep(0.8)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()

    def command(self, cmd: str, timeout_s: float = 6.0, expect: str = "any") -> Dict[str, Any]:
        if not self._ser or not self._ser.is_open:
            raise RuntimeError("serial port not open")
        self._ser.reset_input_buffer()
        self._ser.write((cmd + "\r\n").encode())
        self._ser.flush()

        deadline = time.time() + timeout_s
        last_line = ""
        while time.time() < deadline:
            raw = self._ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            last_line = line
            print(f"[{self.port}] {line}")
            if line and line[0] in ("{", "[") and line[-1] in ("}", "]"):
                if expect in {"any", "json"}:
                    try:
                        return json.loads(line)
                    except json.JSONDecodeError:
                        continue
                continue
            if line.startswith("OK ") or line.startswith("ERR "):
                if expect in {"any", "ack"}:
                    return {"ok": line.startswith("OK "), "line": line}
                continue
            if line == "PONG":
                if expect in {"any", "pong", "ack"}:
                    return {"ok": True, "result": "PONG"}
                continue
        raise RuntimeError(f"timeout on command '{cmd}' last='{last_line}'")

    def sync(self, retries: int = 6) -> None:
        last_error = ""
        for _ in range(retries):
            try:
                self.command("PING", timeout_s=2.0, expect="pong")
                return
            except Exception as exc:  # pragma: no cover - hardware timing
                last_error = str(exc)
                time.sleep(0.5)
        raise RuntimeError(f"serial sync failed: {last_error}")


def resolve_a252_port(explicit_port: str | None) -> str:
    if explicit_port:
        return explicit_port

    preferred_patterns = (
        "/dev/cu.usbserial*",
        "/dev/tty.usbserial*",
    )

    for pattern in preferred_patterns:
        candidates = sorted(glob.glob(pattern))
        if candidates:
            print(f"[hw_validation] A252 locked to USB-Serial port: {candidates[0]}")
            return candidates[0]

    raise RuntimeError(
        "No USB-Serial port found for A252. Expected /dev/tty.usbserial* or /dev/cu.usbserial*."
    )


def fetch_json(url: str) -> Dict[str, Any]:
    req = request.Request(url, method="GET")
    with request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))


def post_json(url: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    raw = json.dumps(payload)
    req = request.Request(
        url,
        method="POST",
        data=raw.encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    try:
        with request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except error.HTTPError as exc:
        if exc.code != 400:
            raise
        # Fallback for endpoints implemented with AsyncWebServer "plain" body extraction.
        fallback = request.Request(
            url,
            method="POST",
            data=parse.urlencode({"plain": raw}).encode("utf-8"),
            headers={"Content-Type": "application/x-www-form-urlencoded"},
        )
        with request.urlopen(fallback, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))


def scenario_serial_smoke(
    dev: SerialEndpoint,
    strict_serial_smoke: bool,
    allow_capture_fail_when_disabled: bool,
) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["ping"] = dev.command("PING", expect="pong")
        details["status"] = dev.command("STATUS", expect="json")
        details["call"] = dev.command("CALL", expect="ack")
        details["capture_start"] = dev.command("CAPTURE_START", expect="ack")
        details["capture_stop"] = dev.command("CAPTURE_STOP", expect="ack")
        details["reset_metrics"] = dev.command("RESET_METRICS", expect="ack")
        state, required_checks, failed_checks, warnings = evaluate_serial_smoke_contract(
            details,
            strict_serial_smoke=strict_serial_smoke,
            allow_capture_fail_when_disabled=allow_capture_fail_when_disabled,
        )
        details["required_checks"] = required_checks
        details["failed_checks"] = failed_checks
        details["warnings"] = warnings
        return ScenarioResult("serial_smoke", state, details)
    except Exception as exc:
        return ScenarioResult("serial_smoke", "FAIL", {"error": str(exc)})


def _is_success_response(resp: Dict[str, Any]) -> bool:
    if "line" in resp:
        line = str(resp.get("line")).strip().upper()
        if line.startswith("OK "):
            return True
        if line.startswith("ERR "):
            return False
        return False
    if "ok" in resp:
        return bool(resp.get("ok"))
    return True


def _has_espnow_capability(resp: Dict[str, Any]) -> bool:
    if not isinstance(resp, dict):
        return False
    if not bool(resp.get("ready")):
        return False
    peers = resp.get("peer_count")
    return isinstance(peers, int) and peers > 0


def _normalize_command(command: str) -> str:
    return command.strip().upper()


def _is_soft_unsupported(command: str, resp: Dict[str, Any]) -> bool:
    normalized_command = _normalize_command(command)
    if normalized_command not in OPTIONAL_SERIAL_COMMANDS:
        return False

    if not isinstance(resp, dict):
        return False

    if isinstance(resp.get("line"), str):
        line = resp["line"].strip().upper()
        return line.startswith("ERR UNSUPPORTED_COMMAND") or "UNSUPPORTED" in line

    if isinstance(resp.get("code"), str):
        code = resp["code"].strip().upper()
        return "UNSUPPORTED" in code

    return False


def _is_acceptable_response(command: str, resp: Dict[str, Any], required: bool) -> bool:
    if required:
        return _is_success_response(resp)
    if _is_soft_unsupported(command, resp):
        return True
    return _is_success_response(resp)


def _extract_capture_enabled(status_payload: Dict[str, Any]) -> bool:
    if not isinstance(status_payload, dict):
        return True
    config = status_payload.get("config")
    if not isinstance(config, dict):
        return True
    audio = config.get("audio")
    if not isinstance(audio, dict):
        return True
    value = audio.get("enable_capture")
    if isinstance(value, bool):
        return value
    return True


def evaluate_serial_smoke_contract(
    details: Dict[str, Any],
    *,
    strict_serial_smoke: bool,
    allow_capture_fail_when_disabled: bool,
) -> tuple[str, List[str], List[str], List[str]]:
    warnings: List[str] = []
    failed_checks: List[str] = []

    ping_ok = bool(details.get("ping", {}).get("ok"))
    status_payload = details.get("status", {})
    status_ok = isinstance(status_payload, dict) and "telephony" in status_payload
    call_ok = _is_success_response(details.get("call", {}))
    capture_start_ok = _is_success_response(details.get("capture_start", {}))
    capture_stop_ok = _is_success_response(details.get("capture_stop", {}))
    reset_metrics_ok = _is_success_response(details.get("reset_metrics", {}))

    capture_enabled = _extract_capture_enabled(status_payload if isinstance(status_payload, dict) else {})
    capture_start_required = capture_enabled or (not allow_capture_fail_when_disabled)

    required_checks: List[str] = ["PING", "STATUS", "CALL", "CAPTURE_STOP", "RESET_METRICS"]
    if capture_start_required:
        required_checks.append("CAPTURE_START")

    if not ping_ok:
        failed_checks.append("PING")
    if not status_ok:
        failed_checks.append("STATUS")
    if not call_ok:
        failed_checks.append("CALL")
    if not capture_stop_ok:
        failed_checks.append("CAPTURE_STOP")
    if not reset_metrics_ok:
        failed_checks.append("RESET_METRICS")
    if capture_start_required and not capture_start_ok:
        failed_checks.append("CAPTURE_START")

    if not capture_enabled and not capture_start_ok:
        if allow_capture_fail_when_disabled:
            warnings.append("capture_start_failed_capture_disabled")
        else:
            warnings.append("capture_start_required_even_when_capture_disabled")

    if strict_serial_smoke:
        return ("PASS" if not failed_checks else "FAIL", required_checks, failed_checks, warnings)

    minimum_failures = [check for check in failed_checks if check in {"PING", "STATUS"}]
    if failed_checks and not minimum_failures:
        warnings.append("strict_serial_smoke_disabled")
    return ("PASS" if not minimum_failures else "FAIL", required_checks, failed_checks, warnings)


def _quote_arg(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def _command_with_retry(
    dev: SerialEndpoint,
    command: str,
    *,
    expect: str = "any",
    timeout_s: float = 6.0,
    attempts: int = 1,
    retry_delay_s: float = 0.5,
) -> Dict[str, Any]:
    last_error: Optional[Exception] = None
    effective_attempts = max(1, attempts)
    for attempt in range(effective_attempts):
        try:
            return dev.command(command, timeout_s=timeout_s, expect=expect)
        except Exception as exc:
            last_error = exc
            if attempt + 1 >= effective_attempts:
                break
            time.sleep(retry_delay_s)
    raise RuntimeError(str(last_error) if last_error else f"command failed: {command}")


def scenario_serial_network(dev: SerialEndpoint, wifi_ssid: str, wifi_password: str) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["wifi_status_before"] = dev.command("WIFI_STATUS", expect="json")
        # Wi-Fi scan can be slow right after boot/flash; retry with a longer timeout.
        details["wifi_scan"] = _command_with_retry(
            dev,
            "WIFI_SCAN",
            expect="any",
            timeout_s=12.0,
            attempts=2,
            retry_delay_s=1.0,
        )
        if wifi_ssid:
            already_connected = (
                bool(details["wifi_status_before"].get("connected"))
                and str(details["wifi_status_before"].get("ssid", "")) == wifi_ssid
            )
            if already_connected:
                details["wifi_connect"] = {"ok": True, "line": "SKIP WIFI_CONNECT already_connected"}
                details["wifi_status_after"] = details["wifi_status_before"]
            else:
                details["wifi_connect"] = dev.command(
                    f"WIFI_CONNECT {_quote_arg(wifi_ssid)} {_quote_arg(wifi_password)}",
                    timeout_s=20.0,
                    expect="ack",
                )
                time.sleep(2.0)
                details["wifi_status_after"] = dev.command("WIFI_STATUS", expect="json")
        details["espnow_status"] = _command_with_retry(
            dev,
            "ESPNOW_STATUS",
            expect="any",
            timeout_s=8.0,
            attempts=2,
            retry_delay_s=0.5,
        )

        checks = [
            ("WIFI_STATUS", details["wifi_status_before"], True),
            ("WIFI_SCAN", details["wifi_scan"], False),
            ("ESPNOW_STATUS", details["espnow_status"], True),
        ]
        if wifi_ssid and "wifi_status_after" in details:
            if "wifi_connect" in details:
                checks.append(("WIFI_CONNECT", details["wifi_connect"], True))
            checks.append(("WIFI_STATUS", details["wifi_status_after"], True))

        ok = True
        for command, value, required in checks:
            if command == "ESPNOW_STATUS" and not _has_espnow_capability(value):
                ok = False
                break
            if not _is_acceptable_response(command, value, required):
                ok = False
                break

        if ok and wifi_ssid and "wifi_status_after" in details:
            wifi_after = details["wifi_status_after"]
            if not (
                isinstance(wifi_after, dict)
                and bool(wifi_after.get("connected"))
                and str(wifi_after.get("ssid", "")) == wifi_ssid
            ):
                ok = False
        return ScenarioResult("serial_network_stack", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_network_stack", "FAIL", {"error": str(exc), **details})


def _extract_hook_state(status_payload: Dict[str, Any]) -> str:
    if not isinstance(status_payload, dict):
        return ""
    telephony = status_payload.get("telephony")
    if not isinstance(telephony, dict):
        return ""
    hook = telephony.get("hook")
    if not isinstance(hook, str):
        return ""
    value = hook.strip().upper()
    if value in {"ON_HOOK", "OFF_HOOK"}:
        return value
    return ""


def _extract_dial_tone_active(status_payload: Dict[str, Any]) -> Optional[bool]:
    if not isinstance(status_payload, dict):
        return None
    audio = status_payload.get("audio")
    if not isinstance(audio, dict):
        return None
    value = audio.get("dial_tone_active")
    if isinstance(value, bool):
        return value
    return None


def scenario_serial_hook_ring_audio(
    dev: SerialEndpoint,
    *,
    require_hook_toggle: bool,
    hook_observe_seconds: int,
) -> ScenarioResult:
    details: Dict[str, Any] = {}
    observed_hooks: List[str] = []

    try:
        details["status_before"] = dev.command("STATUS", expect="json")
        initial_hook = _extract_hook_state(details["status_before"])
        if initial_hook:
            observed_hooks.append(initial_hook)

        details["ring"] = dev.command("RING", expect="ack")
        details["tone_on"] = dev.command("TONE_ON", expect="ack")
        time.sleep(0.4)
        details["status_tone_on"] = dev.command("STATUS", expect="json")
        hook_after_tone_on = _extract_hook_state(details["status_tone_on"])
        if hook_after_tone_on:
            observed_hooks.append(hook_after_tone_on)

        details["tone_off"] = dev.command("TONE_OFF", expect="ack")
        time.sleep(0.3)
        details["status_tone_off"] = dev.command("STATUS", expect="json")
        hook_after_tone_off = _extract_hook_state(details["status_tone_off"])
        if hook_after_tone_off:
            observed_hooks.append(hook_after_tone_off)

        observe_seconds = max(0, hook_observe_seconds)
        poll_deadline = time.time() + float(observe_seconds)
        while time.time() < poll_deadline:
            poll_status = dev.command("STATUS", expect="json")
            hook = _extract_hook_state(poll_status)
            if hook:
                observed_hooks.append(hook)
            time.sleep(0.8)

        unique_hooks = sorted(set(observed_hooks))
        details["hook_values_observed"] = unique_hooks
        details["hook_observe_seconds"] = observe_seconds

        ring_ok = _is_success_response(details["ring"])
        tone_on_ok = _is_success_response(details["tone_on"])
        tone_off_ok = _is_success_response(details["tone_off"])
        tone_active_after_on = _extract_dial_tone_active(details["status_tone_on"]) is True
        tone_inactive_after_off = _extract_dial_tone_active(details["status_tone_off"]) is False

        if require_hook_toggle:
            required_hooks = ["ON_HOOK", "OFF_HOOK"]
            hook_ok = all(state in unique_hooks for state in required_hooks)
            details["required_hook_states"] = required_hooks
        else:
            hook_ok = len(unique_hooks) > 0
            details["required_hook_states"] = ["ON_HOOK|OFF_HOOK"]

        details["checks"] = {
            "ring_ok": ring_ok,
            "tone_on_ok": tone_on_ok,
            "tone_active_after_on": tone_active_after_on,
            "tone_off_ok": tone_off_ok,
            "tone_inactive_after_off": tone_inactive_after_off,
            "hook_ok": hook_ok,
        }

        ok = (
            ring_ok
            and tone_on_ok
            and tone_active_after_on
            and tone_off_ok
            and tone_inactive_after_off
            and hook_ok
        )
        return ScenarioResult("serial_hook_ring_audio", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_hook_ring_audio", "FAIL", {"error": str(exc), **details})


def scenario_http(base_url: str) -> ScenarioResult:
    details: Dict[str, Any] = {"base_url": base_url}
    try:
        details["status"] = fetch_json(base_url.rstrip("/") + "/api/status")
        details["wifi"] = fetch_json(base_url.rstrip("/") + "/api/network/wifi")
        details["espnow"] = fetch_json(base_url.rstrip("/") + "/api/network/espnow")
        details["control_call"] = post_json(base_url.rstrip("/") + "/api/control", {"action": "CALL"})
        return ScenarioResult("http_endpoints", "PASS", details)
    except error.HTTPError as exc:
        return ScenarioResult("http_endpoints", "FAIL", {"error": f"HTTP {exc.code}", **details})
    except Exception as exc:
        return ScenarioResult("http_endpoints", "FAIL", {"error": str(exc), **details})


def scenario_manual(name: str, state: str, note: str) -> ScenarioResult:
    if state not in VALID_STATES:
        state = "MANUAL_SKIP"
    return ScenarioResult(name, state, {"note": note})


def write_reports(results: List[ScenarioResult], report_json: Path, report_md: Path) -> None:
    ensure_parent(report_json)
    ensure_parent(report_md)

    overall_passed = all(item.state not in {"FAIL", "MANUAL_FAIL"} for item in results)
    payload = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "overall_passed": overall_passed,
        "results": [{"name": x.name, "state": x.state, "details": x.details} for x in results],
    }
    report_json.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = [
        "# Rapport validation HW (A252)",
        "",
        f"- Date UTC: {payload['timestamp_utc']}",
        f"- Verdict global: {'PASS' if overall_passed else 'FAIL'}",
        "",
        "| Scénario | État | Détails |",
        "|---|---|---|",
    ]
    for item in results:
        details = json.dumps(item.details, ensure_ascii=False)
        lines.append(f"| {item.name} | {item.state} | `{details}` |")
    report_md.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="RTC_BL_PHONE A252 validation runner")
    parser.add_argument(
        "--port-a252",
        default="",
        help=(
            "serial port for A252 target. If omitted, auto-detects first /dev/*usbserial* on macOS/"
            "Linux."
        ),
    )
    parser.add_argument("--baud", type=int, default=115200, help="serial baudrate")
    parser.add_argument("--flash", action="store_true", help="build and upload firmware before tests")
    parser.add_argument("--base-url", default="", help="optional A252 base URL (http://ip)")
    parser.add_argument("--wifi-ssid", default="", help="optional SSID for WIFI_CONNECT test")
    parser.add_argument("--wifi-password", default="", help="optional password for WIFI_CONNECT test")
    parser.add_argument("--report-json", default="docs/rapport_hw.json", help="output JSON report path")
    parser.add_argument(
        "--report-md", default="docs/rapport_tests_fonctionnels.md", help="output Markdown report path"
    )
    parser.add_argument("--manual-hook", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-ring", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-audio", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-hfp", default="MANUAL_SKIP", choices=sorted(VALID_STATES))
    parser.add_argument("--manual-note", default="", help="optional shared note for manual checks")
    parser.add_argument(
        "--strict-serial-smoke",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="if enabled, fail serial_smoke when required command checks fail (default: enabled)",
    )
    parser.add_argument(
        "--allow-capture-fail-when-disabled",
        action=argparse.BooleanOptionalAction,
        default=True,
        help=(
            "if enabled, CAPTURE_START failure is tolerated when STATUS reports audio.enable_capture=false "
            "(default: enabled)"
        ),
    )
    parser.add_argument(
        "--require-hook-toggle",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="if enabled, serial_hook_ring_audio requires both ON_HOOK and OFF_HOOK states",
    )
    parser.add_argument(
        "--hook-observe-seconds",
        type=int,
        default=45,
        help="hook observation window in seconds for serial_hook_ring_audio (default: 45)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    resolved_port = resolve_a252_port(args.port_a252.strip() or None)

    if args.flash:
        run_cmd(["pio", "run", "-e", "esp32dev"])
        run_cmd(["pio", "run", "-e", "esp32dev", "-t", "upload", "--upload-port", resolved_port])

    results: List[ScenarioResult] = []
    network_result: Optional[ScenarioResult] = None

    try:
        with SerialEndpoint(resolved_port, args.baud) as dev:
            dev.sync()
            results.append(
                scenario_serial_smoke(
                    dev,
                    strict_serial_smoke=args.strict_serial_smoke,
                    allow_capture_fail_when_disabled=args.allow_capture_fail_when_disabled,
                )
            )
            results.append(
                scenario_serial_hook_ring_audio(
                    dev,
                    require_hook_toggle=args.require_hook_toggle,
                    hook_observe_seconds=args.hook_observe_seconds,
                )
            )
            network_result = scenario_serial_network(dev, args.wifi_ssid, args.wifi_password)
            results.append(network_result)
    except Exception as exc:
        results.append(ScenarioResult("serial_runner", "FAIL", {"error": str(exc)}))

    runtime_base_url = args.base_url.strip()
    if network_result and isinstance(network_result.details, dict):
        wifi_after = network_result.details.get("wifi_status_after")
        if isinstance(wifi_after, dict) and wifi_after.get("connected") and wifi_after.get("ip"):
            runtime_base_url = f"http://{wifi_after['ip']}"

    if runtime_base_url:
        results.append(scenario_http(runtime_base_url))
    else:
        results.append(ScenarioResult("http_endpoints", "MANUAL_SKIP", {"note": "base URL not provided"}))

    note = args.manual_note or "validated manually"
    results.append(scenario_manual("manual_hook_transition", args.manual_hook, note))
    results.append(scenario_manual("manual_ring_behavior", args.manual_ring, note))
    results.append(scenario_manual("manual_audio_path", args.manual_audio, note))
    results.append(scenario_manual("manual_hfp_pairing", args.manual_hfp, note))

    write_reports(results, Path(args.report_json), Path(args.report_md))
    return 0 if all(item.state not in {"FAIL", "MANUAL_FAIL"} for item in results) else 1


if __name__ == "__main__":
    sys.exit(main())
