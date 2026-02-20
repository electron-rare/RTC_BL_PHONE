#!/usr/bin/env python3
"""A252-only hardware validation runner (without bench controller)."""

from __future__ import annotations

import argparse
import json
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


VALID_STATES = {"PASS", "FAIL", "MANUAL_PASS", "MANUAL_FAIL", "MANUAL_SKIP"}


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
        time.sleep(0.3)
        self._ser.reset_input_buffer()
        self._ser.reset_output_buffer()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._ser and self._ser.is_open:
            self._ser.close()

    def command(self, cmd: str, timeout_s: float = 6.0) -> Dict[str, Any]:
        if not self._ser or not self._ser.is_open:
            raise RuntimeError("serial port not open")
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
            if line.startswith("{") and line.endswith("}"):
                try:
                    return json.loads(line)
                except json.JSONDecodeError:
                    continue
            if line.startswith("OK ") or line.startswith("ERR "):
                return {"ok": line.startswith("OK "), "line": line}
            if line == "PONG":
                return {"ok": True, "result": "PONG"}
        raise RuntimeError(f"timeout on command '{cmd}' last='{last_line}'")


def fetch_json(url: str) -> Dict[str, Any]:
    req = request.Request(url, method="GET")
    with request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))


def post_json(url: str, payload: Dict[str, Any]) -> Dict[str, Any]:
    req = request.Request(
        url,
        method="POST",
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
    )
    with request.urlopen(req, timeout=5) as resp:
        return json.loads(resp.read().decode("utf-8"))


def scenario_serial_smoke(dev: SerialEndpoint) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["ping"] = dev.command("PING")
        details["status"] = dev.command("STATUS")
        details["call"] = dev.command("CALL")
        details["capture_start"] = dev.command("CAPTURE_START")
        details["capture_stop"] = dev.command("CAPTURE_STOP")
        details["reset_metrics"] = dev.command("RESET_METRICS")
        ok = bool(details["ping"].get("ok")) and "telephony" in details["status"]
        return ScenarioResult("serial_smoke", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_smoke", "FAIL", {"error": str(exc)})


def _is_success_response(resp: Dict[str, Any]) -> bool:
    if "ok" in resp:
        return bool(resp.get("ok"))
    return True


def scenario_serial_network(dev: SerialEndpoint, wifi_ssid: str, wifi_password: str) -> ScenarioResult:
    details: Dict[str, Any] = {}
    try:
        details["wifi_status_before"] = dev.command("WIFI_STATUS")
        details["wifi_scan"] = dev.command("WIFI_SCAN")
        if wifi_ssid:
            details["wifi_connect"] = dev.command(f"WIFI_CONNECT {wifi_ssid} {wifi_password}")
            time.sleep(1.0)
            details["wifi_status_after"] = dev.command("WIFI_STATUS")
        details["mqtt_status"] = dev.command("MQTT_STATUS")
        details["espnow_status"] = dev.command("ESPNOW_STATUS")
        details["bt_status"] = dev.command("BT_STATUS")
        ok = True
        for value in details.values():
            if isinstance(value, dict) and not _is_success_response(value):
                ok = False
                break
        return ScenarioResult("serial_network_stack", "PASS" if ok else "FAIL", details)
    except Exception as exc:
        return ScenarioResult("serial_network_stack", "FAIL", {"error": str(exc), **details})


def scenario_http(base_url: str) -> ScenarioResult:
    details: Dict[str, Any] = {"base_url": base_url}
    try:
        details["status"] = fetch_json(base_url.rstrip("/") + "/api/status")
        details["wifi"] = fetch_json(base_url.rstrip("/") + "/api/network/wifi")
        details["mqtt"] = fetch_json(base_url.rstrip("/") + "/api/network/mqtt")
        details["espnow"] = fetch_json(base_url.rstrip("/") + "/api/network/espnow")
        details["bluetooth"] = fetch_json(base_url.rstrip("/") + "/api/bluetooth")
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
    parser.add_argument("--port-a252", required=True, help="serial port for A252 target")
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
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.flash:
        run_cmd(["pio", "run", "-e", "esp32dev"])
        run_cmd(["pio", "run", "-e", "esp32dev", "-t", "upload", "--upload-port", args.port_a252])

    results: List[ScenarioResult] = []

    try:
        with SerialEndpoint(args.port_a252, args.baud) as dev:
            results.append(scenario_serial_smoke(dev))
            results.append(scenario_serial_network(dev, args.wifi_ssid, args.wifi_password))
    except Exception as exc:
        results.append(ScenarioResult("serial_runner", "FAIL", {"error": str(exc)}))

    if args.base_url:
        results.append(scenario_http(args.base_url))
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
