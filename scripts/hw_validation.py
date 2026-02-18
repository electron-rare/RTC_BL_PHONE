#!/usr/bin/env python3
"""Local hardware validation runner for A252 + ESP32-S3."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional
from urllib import error, request

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    serial = None


@dataclass
class ScenarioResult:
    name: str
    passed: bool
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
        time.sleep(0.3)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()

    def command(
        self,
        cmd: str,
        *,
        timeout_s: float = 5.0,
        expect_json: bool = False,
        expected_prefixes: Optional[List[str]] = None,
    ) -> Any:
        if self._ser is None:
            raise RuntimeError("serial endpoint is not open")

        self._ser.write((cmd + "\n").encode("utf-8"))
        self._ser.flush()

        deadline = time.monotonic() + timeout_s
        last_line = ""
        while time.monotonic() < deadline:
            raw = self._ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            last_line = line
            print(f"[{self.port}] {line}")

            if expect_json:
                try:
                    return json.loads(line)
                except json.JSONDecodeError:
                    continue

            if expected_prefixes is None:
                return line
            if any(line.startswith(prefix) for prefix in expected_prefixes):
                return line

        raise RuntimeError(f"Timeout waiting response to '{cmd}' on {self.port}; last='{last_line}'")


def parse_latency_ms(value: Any, fallback_ms: int) -> int:
    if isinstance(value, (int, float)):
        return int(value)
    if isinstance(value, str):
        match = re.search(r"(\d+(\.\d+)?)", value)
        if match:
            return int(float(match.group(1)))
    return fallback_ms


def fetch_http_status(base_url: str) -> Dict[str, Any]:
    url = base_url.rstrip("/") + "/api/status"
    req = request.Request(url, method="GET")
    with request.urlopen(req, timeout=5) as response:
        payload = response.read().decode("utf-8")
    return json.loads(payload)


def check_web_endpoint(base_url: str, path: str, method: str = "GET", body: Optional[Dict[str, Any]] = None) -> int:
    url = base_url.rstrip("/") + path
    data = None
    headers = {}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = request.Request(url, data=data, headers=headers, method=method)
    try:
        with request.urlopen(req, timeout=5) as response:
            return int(response.status)
    except error.HTTPError as exc:
        return int(exc.code)


def scenario_slic_transition(
    name: str,
    phone: SerialEndpoint,
    bench: SerialEndpoint,
    hook_target: str,
) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        phone.command("CALL", expected_prefixes=["OK", "ERR"], timeout_s=2)
        time.sleep(0.8)
        status_ring = phone.command("STATUS", expect_json=True, timeout_s=3)
        details["ring_state"] = status_ring.get("telephony")

        bench.command(f"HOOK {hook_target} ON", timeout_s=3)
        time.sleep(0.8)
        status_offhook = phone.command("STATUS", expect_json=True, timeout_s=3)
        details["offhook_state"] = status_offhook.get("telephony")

        bench.command(f"HOOK {hook_target} OFF", timeout_s=3)
        time.sleep(0.8)
        status_idle = phone.command("STATUS", expect_json=True, timeout_s=3)
        details["idle_state"] = status_idle.get("telephony")

        passed = (
            details["ring_state"] == "RINGING"
            and details["offhook_state"] in ("PLAYING_MESSAGE", "OFF_HOOK")
            and details["idle_state"] == "IDLE"
        )
        return ScenarioResult(name=name, passed=passed, details=details)
    except Exception as exc:  # pragma: no cover
        details["error"] = str(exc)
        return ScenarioResult(name=name, passed=False, details=details)


def scenario_a252_full_duplex(phone: SerialEndpoint, bench: SerialEndpoint) -> ScenarioResult:
    details: Dict[str, Any] = {"duration_s": 120}
    try:
        phone.command("RESET_METRICS", expected_prefixes=["OK"], timeout_s=2)
        phone.command("CAPTURE_START", expected_prefixes=["OK", "ERR"], timeout_s=3)
        phone.command("PLAY /welcome.wav", expected_prefixes=["OK", "ERR"], timeout_s=3)

        bench.command("AUDIO INJECT START 1000 0.40", timeout_s=3)
        bench.command("MEASURE LATENCY START", timeout_s=3)

        end_time = time.monotonic() + 120
        while time.monotonic() < end_time:
            time.sleep(5)
            phone.command("STATUS", expect_json=True, timeout_s=3)

        latency_line = bench.command("MEASURE LATENCY READ", timeout_s=5)
        bench.command("AUDIO INJECT STOP", timeout_s=3)
        phone.command("CAPTURE_STOP", expected_prefixes=["OK"], timeout_s=3)

        status = phone.command("STATUS", expect_json=True, timeout_s=5)
        details.update(
            {
                "audio_underrun_count": status.get("audio_underrun_count", 0),
                "audio_drop_frames": status.get("audio_drop_frames", 0),
                "audio_last_latency_ms": status.get("audio_last_latency_ms", 0),
                "bench_latency_ms": parse_latency_ms(latency_line, 9999),
                "telephony_state": status.get("telephony", "UNKNOWN"),
            }
        )

        passed = (
            int(details["audio_underrun_count"]) <= 1
            and int(details["audio_drop_frames"]) == 0
            and int(details["bench_latency_ms"]) <= 120
        )
        return ScenarioResult(name="A252 full-duplex", passed=passed, details=details)
    except Exception as exc:  # pragma: no cover
        details["error"] = str(exc)
        return ScenarioResult(name="A252 full-duplex", passed=False, details=details)


def scenario_s3_local(phone: SerialEndpoint, bench: SerialEndpoint) -> ScenarioResult:
    details: Dict[str, Any] = {"duration_s": 20}
    try:
        phone.command("RESET_METRICS", expected_prefixes=["OK"], timeout_s=2)
        phone.command("CALL", expected_prefixes=["OK"], timeout_s=2)
        time.sleep(1.0)
        bench.command("HOOK S3 ON", timeout_s=3)

        phone.command("CAPTURE_START", expected_prefixes=["OK", "ERR"], timeout_s=3)
        bench.command("AUDIO INJECT START 1000 0.40", timeout_s=3)
        bench.command("MEASURE LATENCY START", timeout_s=3)
        time.sleep(20)

        latency_line = bench.command("MEASURE LATENCY READ", timeout_s=5)
        bench.command("AUDIO INJECT STOP", timeout_s=3)
        phone.command("CAPTURE_STOP", expected_prefixes=["OK"], timeout_s=3)
        bench.command("HOOK S3 OFF", timeout_s=3)

        status = phone.command("STATUS", expect_json=True, timeout_s=5)
        details.update(
            {
                "telephony_state": status.get("telephony"),
                "audio_drop_frames": status.get("audio_drop_frames", 0),
                "bench_latency_ms": parse_latency_ms(latency_line, 9999),
            }
        )
        passed = int(details["bench_latency_ms"]) <= 150 and details["telephony_state"] in (
            "PLAYING_MESSAGE",
            "OFF_HOOK",
            "IDLE",
        )
        return ScenarioResult(name="S3 local mode", passed=passed, details=details)
    except Exception as exc:  # pragma: no cover
        details["error"] = str(exc)
        return ScenarioResult(name="S3 local mode", passed=False, details=details)


def scenario_web_access(base_url: Optional[str], label: str) -> ScenarioResult:
    details: Dict[str, Any] = {}
    if not base_url:
        return ScenarioResult(name=f"{label} web endpoints", passed=True, details={"skipped": True})

    try:
        status_payload = fetch_http_status(base_url)
        details["status_code"] = 200
        details["board_profile"] = status_payload.get("board_profile", "UNKNOWN")

        details["config_status"] = check_web_endpoint(base_url, "/api/config", "GET")
        details["logs_status"] = check_web_endpoint(base_url, "/api/logs", "GET")
        details["control_status"] = check_web_endpoint(
            base_url, "/api/control", "POST", {"action": "call"}
        )

        passed = (
            details["config_status"] == 200
            and details["logs_status"] == 200
            and details["control_status"] == 200
        )
        return ScenarioResult(name=f"{label} web endpoints", passed=passed, details=details)
    except Exception as exc:  # pragma: no cover
        details["error"] = str(exc)
        return ScenarioResult(name=f"{label} web endpoints", passed=False, details=details)


def write_reports(results: List[ScenarioResult], report_json: Path, report_md: Path) -> None:
    overall_passed = all(item.passed for item in results)
    payload = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "overall_passed": overall_passed,
        "results": [
            {
                "name": item.name,
                "passed": item.passed,
                "details": item.details,
            }
            for item in results
        ],
    }

    ensure_parent(report_json)
    ensure_parent(report_md)
    report_json.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    lines = [
        "# Rapport validation HW",
        "",
        f"- Date UTC: {payload['timestamp_utc']}",
        f"- Verdict global: {'PASS' if overall_passed else 'FAIL'}",
        "",
        "| Scénario | Verdict | Détails |",
        "|---|---|---|",
    ]
    for item in results:
        details = json.dumps(item.details, ensure_ascii=False)
        verdict = "PASS" if item.passed else "FAIL"
        lines.append(f"| {item.name} | {verdict} | `{details}` |")
    report_md.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="RTC_BL_PHONE hardware validation")
    parser.add_argument("--port-a252", required=True, help="Serial port for A252 target")
    parser.add_argument("--port-s3", required=True, help="Serial port for ESP32-S3 target")
    parser.add_argument("--bench-port", required=True, help="Serial port for bench controller")
    parser.add_argument("--baud", type=int, default=115200, help="UART baudrate")
    parser.add_argument("--flash", action="store_true", help="Build and flash both targets before tests")
    parser.add_argument("--report-json", default="docs/rapport_hw.json", help="JSON report path")
    parser.add_argument(
        "--report-md", default="docs/rapport_tests_fonctionnels.md", help="Markdown report path"
    )
    parser.add_argument("--a252-base-url", default="", help="Optional base URL for A252 web API")
    parser.add_argument("--s3-base-url", default="", help="Optional base URL for S3 web API")
    return parser.parse_args()


def maybe_flash(args: argparse.Namespace) -> None:
    if not args.flash:
        return
    run_cmd(["pio", "run", "-e", "esp32dev", "-e", "esp32-s3-devkitc-1"])
    run_cmd(["pio", "run", "-e", "esp32dev", "-t", "upload", "--upload-port", args.port_a252])
    run_cmd(["pio", "run", "-e", "esp32-s3-devkitc-1", "-t", "upload", "--upload-port", args.port_s3])


def main() -> int:
    args = parse_args()
    maybe_flash(args)

    results: List[ScenarioResult] = []
    try:
        with SerialEndpoint(args.port_a252, args.baud) as dev_a252, SerialEndpoint(
            args.port_s3, args.baud
        ) as dev_s3, SerialEndpoint(args.bench_port, args.baud) as bench:
            dev_a252.command("PING", expected_prefixes=["PONG"], timeout_s=4)
            dev_s3.command("PING", expected_prefixes=["PONG"], timeout_s=4)
            bench.command("PING", timeout_s=3)
            bench.command("RESET", timeout_s=3)

            results.append(scenario_slic_transition("SLIC transition A252", dev_a252, bench, "A252"))
            results.append(scenario_slic_transition("SLIC transition S3", dev_s3, bench, "S3"))
            results.append(scenario_a252_full_duplex(dev_a252, bench))
            results.append(scenario_s3_local(dev_s3, bench))
            results.append(scenario_web_access(args.a252_base_url or None, "A252"))
            results.append(scenario_web_access(args.s3_base_url or None, "S3"))
    except Exception as exc:
        results.append(
            ScenarioResult(
                name="runner",
                passed=False,
                details={"error": str(exc)},
            )
        )

    write_reports(results, Path(args.report_json), Path(args.report_md))
    overall_passed = all(item.passed for item in results)
    return 0 if overall_passed else 1


if __name__ == "__main__":
    sys.exit(main())
