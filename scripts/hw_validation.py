from __future__ import annotations
import glob

# --- Détection automatique des ports série compatibles ---
def detect_serial_ports():
    patterns = [
        '/dev/ttyUSB*', '/dev/ttyACM*', '/dev/cu.usbserial*', '/dev/cu.SLAB_USBtoUART*', '/dev/cu.wchusbserial*', '/dev/cu.usbmodem*'
    ]
    ports = []
    for pat in patterns:
        ports.extend(glob.glob(pat))
    return ports

# --- Identification du chip sur chaque port ---
def reset_to_bootloader(port):
    """Tente de forcer le reset en mode bootloader via DTR/RTS (méthode Espressif)."""
    try:
        import serial
        with serial.Serial(port, baudrate=115200) as ser:
            # DTR = 0, RTS = 1 => EN=0, IO0=1 (reset)
            ser.dtr = False
            ser.rts = True
            time.sleep(0.1)
            # DTR = 1, RTS = 0 => EN=1, IO0=0 (bootloader)
            ser.dtr = True
            ser.rts = False
            time.sleep(0.1)
            # Relâche tout
            ser.dtr = False
            ser.rts = False
            time.sleep(0.1)
    except Exception:
        pass

def identify_chip_on_port(port, baud=115200, timeout=1.0):
    # 1. Tente un reset auto en mode bootloader
    reset_to_bootloader(port)

    # 2. Détection hardware via esptool (API robuste)
    try:
        import esptool
        # Version la plus stricte (port, serial_list, connect_attempts, initial_baud)
        if hasattr(esptool, 'get_default_connected_device'):
            dev = esptool.get_default_connected_device(
                port=port, serial_list=[port], connect_attempts=1, initial_baud=baud
            )
            desc = dev.get_chip_description()
            return desc
        elif hasattr(esptool.ESPLoader, 'detect_chip'):
            chip = esptool.ESPLoader.detect_chip(port=port, baud=baud)
            return chip.get_chip_description()
        else:
            # Fallback très ancien esptool
            return None
    except Exception:
        return None

# --- Sélection automatique des ports pour A252 et S3 ---
def auto_select_ports():
    ports = detect_serial_ports()
    found = {}
    for port in ports:
        chip = identify_chip_on_port(port)
        if chip:
            if 'S3' in chip or 'ESP32-S3' in chip:
                found['s3'] = port
            elif 'A252' in chip or 'ESP32' in chip:
                found['a252'] = port
    return found
# --- Test de coupure/rétablissement WiFi ---
def scenario_wifi_cut_restore(device: SerialEndpoint, ssid: str, password: str) -> ScenarioResult:
    details = {}
    try:
        # 1. Coupure WiFi
        device.command("WIFI DISCONNECT", expected_prefixes=["OK"], timeout_s=3)
        time.sleep(2)
        wifi_status_cut = device.command("WIFI STATUS", expect_json=True, timeout_s=3)
        details["wifi_after_cut"] = wifi_status_cut
        # 2. Rétablissement WiFi
        resp = device.command(f"WIFI CONNECT {ssid} {password}", expect_json=True, timeout_s=8)
        details["wifi_reconnect"] = resp
        wifi_status_re = device.command("WIFI STATUS", expect_json=True, timeout_s=3)
        details["wifi_after_restore"] = wifi_status_re
        passed = (wifi_status_cut.get("connected") is False) and (wifi_status_re.get("connected") is True)
        return ScenarioResult(name=f"WiFi cut/restore {device.port}", passed=passed, details=details)
    except Exception as exc:
        return ScenarioResult(name=f"WiFi cut/restore {device.port}", passed=False, details={"error": str(exc)})
# --- Test de connexion WiFi ---
def scenario_wifi_connect(device: SerialEndpoint, ssid: str, password: str) -> ScenarioResult:
    try:
        resp = device.command(f"WIFI CONNECT {ssid} {password}", expect_json=True, timeout_s=8)
        passed = resp.get("connected") is True
        return ScenarioResult(name=f"WiFi connect {device.port}", passed=passed, details=resp)
    except Exception as exc:
        return ScenarioResult(name=f"WiFi connect {device.port}", passed=False, details={"error": str(exc)})

# --- Test de connexion BLE ---
def scenario_ble_connect(device: SerialEndpoint, device_name: str) -> ScenarioResult:
    try:
        resp = device.command(f"BT CONNECT {device_name}", expect_json=True, timeout_s=8)
        passed = resp.get("connected") is True
        return ScenarioResult(name=f"BLE connect {device.port}", passed=passed, details=resp)
    except Exception as exc:
        return ScenarioResult(name=f"BLE connect {device.port}", passed=False, details={"error": str(exc)})
# --- Test de coupure/rétablissement WiFi et fallback BLE ---
def scenario_wifi_cut_fallback_ble(device: SerialEndpoint) -> ScenarioResult:
    details = {}
    try:
        # 1. Forcer la coupure WiFi
        device.command("WIFI DISCONNECT", expected_prefixes=["OK"], timeout_s=3)
        time.sleep(2)
        wifi_status = device.command("WIFI STATUS", expect_json=True, timeout_s=3)
        details["wifi_after_cut"] = wifi_status
        # 2. Tenter fallback BLE
        ble_resp = device.command("BT ENABLE", expect_json=True, timeout_s=4)
        details["ble_enable"] = ble_resp
        ble_status = device.command("BT STATUS", expect_json=True, timeout_s=3)
        details["ble_status"] = ble_status
        passed = (wifi_status.get("connected") is False) and ble_status.get("enabled") is True
        return ScenarioResult(name=f"WiFi cut + fallback BLE {device.port}", passed=passed, details=details)
    except Exception as exc:
        return ScenarioResult(name=f"WiFi cut + fallback BLE {device.port}", passed=False, details={"error": str(exc)})
# --- Scénario de scan WiFi ---
def scenario_wifi_scan(device: SerialEndpoint, ssid_expected: Optional[str] = None) -> ScenarioResult:
    try:
        resp = device.command("WIFI SCAN", expect_json=True, timeout_s=8)
        found = False
        if isinstance(resp, dict) and "scan" in resp:
            found = any((ssid_expected is None or ap.get("ssid") == ssid_expected) for ap in resp["scan"])
        passed = found if ssid_expected else bool(resp.get("scan"))
        details = {"found": found, "scan": resp.get("scan", [])}
        return ScenarioResult(name=f"WiFi scan {device.port}", passed=passed, details=details)
    except Exception as exc:
        return ScenarioResult(name=f"WiFi scan {device.port}", passed=False, details={"error": str(exc)})

# --- Scénario de scan BLE ---
def scenario_ble_scan(device: SerialEndpoint, device_expected: Optional[str] = None) -> ScenarioResult:
    try:
        resp = device.command("BT SCAN", expect_json=True, timeout_s=8)
        found = False
        if isinstance(resp, dict) and "scan" in resp:
            found = any((device_expected is None or d.get("name") == device_expected) for d in resp["scan"])
        passed = found if device_expected else bool(resp.get("scan"))
        details = {"found": found, "scan": resp.get("scan", [])}
        return ScenarioResult(name=f"BLE scan {device.port}", passed=passed, details=details)
    except Exception as exc:
        return ScenarioResult(name=f"BLE scan {device.port}", passed=False, details={"error": str(exc)})

# --- Test de reconnexion WiFi ---
def scenario_wifi_reconnect(device: SerialEndpoint) -> ScenarioResult:
    try:
        device.command("WIFI DISCONNECT", expected_prefixes=["OK"], timeout_s=3)
        time.sleep(2)
        resp = device.command("WIFI RECONNECT", expect_json=True, timeout_s=6)
        passed = resp.get("connected") is True
        return ScenarioResult(name=f"WiFi reconnect {device.port}", passed=passed, details=resp)
    except Exception as exc:
        return ScenarioResult(name=f"WiFi reconnect {device.port}", passed=False, details={"error": str(exc)})

# --- Vérification sécurisation API ---
def scenario_api_security(base_url: str) -> ScenarioResult:
    details = {}
    try:
        # Test accès sans authentification (doit échouer si auth requise)
        req = request.Request(base_url.rstrip("/") + "/api/config", method="POST")
        try:
            with request.urlopen(req, timeout=3) as response:
                details["unauth_status"] = response.status
        except error.HTTPError as exc:
            details["unauth_status"] = exc.code
        # Test CORS (optionnel, dépend du serveur)
        req = request.Request(base_url.rstrip("/") + "/api/config", method="OPTIONS")
        try:
            with request.urlopen(req, timeout=3) as response:
                details["cors_status"] = response.status
        except error.HTTPError as exc:
            details["cors_status"] = exc.code
        # Critère : accès POST non autorisé sans auth (401/403 attendu)
        passed = details["unauth_status"] in (401, 403)
        return ScenarioResult(name=f"API security {base_url}", passed=passed, details=details)
    except Exception as exc:
        return ScenarioResult(name=f"API security {base_url}", passed=False, details={"error": str(exc)})

#!/usr/bin/env python3
"""Local hardware validation runner for A252 + ESP32-S3."""
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
        expected_prefixes: Optional[list] = None,
    ):
        """Envoie une commande sur le port série et attend une réponse."""
        if not self._ser or not self._ser.is_open:
            raise RuntimeError("Serial port not open")
        self._ser.write((cmd + "\r\n").encode())
        self._ser.flush()
        import time, json
        last_line = ""
        deadline = time.time() + timeout_s
        while time.time() < deadline:
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

# --- Ajout des scénarios WiFi et Bluetooth ---
def scenario_wifi_status(device: SerialEndpoint) -> ScenarioResult:
    try:
        resp = device.command("WIFI STATUS", expect_json=True, timeout_s=3)
        passed = resp.get("connected") is True
        return ScenarioResult(name=f"WiFi status {device.port}", passed=passed, details=resp)
    except Exception as exc:
        return ScenarioResult(name=f"WiFi status {device.port}", passed=False, details={"error": str(exc)})

def scenario_bt_status(device: SerialEndpoint) -> ScenarioResult:
    try:
        resp = device.command("BT STATUS", expect_json=True, timeout_s=3)
        passed = resp.get("enabled") is True
        return ScenarioResult(name=f"Bluetooth status {device.port}", passed=passed, details=resp)
    except Exception as exc:
        return ScenarioResult(name=f"Bluetooth status {device.port}", passed=False, details={"error": str(exc)})


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
    parser.add_argument("--port-a252", required=False, help="Serial port for A252 target (auto si absent)")
    parser.add_argument("--port-s3", required=False, help="Serial port for ESP32-S3 target (auto si absent)")
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



def reset_serial_port(port):
    try:
        import serial
        with serial.Serial(port, baudrate=115200, timeout=0.2) as ser:
            ser.dtr = False
            ser.rts = False
            ser.reset_input_buffer()
            ser.reset_output_buffer()
        print(f"[INFO] Reset logiciel du port {port} OK.")
    except Exception as e:
        print(f"[WARN] Reset logiciel du port {port} échoué: {e}")

    args = parse_args()
    # Détection automatique des ports si non fournis

    import time
    if not args.port_a252 or not args.port_s3:
        ports = auto_select_ports()
        if not args.port_a252 and 'a252' in ports:
            args.port_a252 = ports['a252']
            print(f"[AUTO] Port A252 détecté : {args.port_a252}")
        if not args.port_s3 and 's3' in ports:
            args.port_s3 = ports['s3']
            print(f"[AUTO] Port S3 détecté : {args.port_s3}")
        if not args.port_a252 or not args.port_s3:
            raise RuntimeError("Impossible de détecter automatiquement les ports série pour A252 et S3.")

    # Pause pour s'assurer que les ports sont bien libérés
    print("[INFO] Pause pour libérer les ports série...")
    time.sleep(2)

    # Build et upload systématique avant validation (robuste)
    print("[BUILD] Compilation des firmwares A252 et S3...")
    run_cmd(["pio", "run", "-e", "esp32dev", "-e", "esp32-s3-devkitc-1"])
    print(f"[RESET] Logiciel du port {args.port_a252} avant upload...")
    reset_serial_port(args.port_a252)
    time.sleep(1)
    print(f"[UPLOAD] Flash A252 sur {args.port_a252} ...")
    run_cmd(["pio", "run", "-e", "esp32dev", "-t", "upload", "--upload-port", args.port_a252])
    print(f"[RESET] Logiciel du port {args.port_s3} avant upload...")
    reset_serial_port(args.port_s3)
    time.sleep(1)
    print(f"[UPLOAD] Flash S3 sur {args.port_s3} ...")
    run_cmd(["pio", "run", "-e", "esp32-s3-devkitc-1", "-t", "upload", "--upload-port", args.port_s3])

    results: List[ScenarioResult] = []
    try:
        with SerialEndpoint(args.port_a252, args.baud) as dev_a252, SerialEndpoint(
            args.port_s3, args.baud
        ) as dev_s3, SerialEndpoint(args.bench_port, args.baud) as bench:
            dev_a252.command("PING", expected_prefixes=["PONG"], timeout_s=4)
            dev_s3.command("PING", expected_prefixes=["PONG"], timeout_s=4)
            bench.command("PING", timeout_s=3)
            bench.command("RESET", timeout_s=3)


            # --- Tests WiFi/BLE avancés ---
            results.append(scenario_wifi_status(dev_a252))
            results.append(scenario_wifi_scan(dev_a252))
            # Test explicite de connexion WiFi (SSID/PASS à adapter)
            results.append(scenario_wifi_connect(dev_a252, "TestSSID", "TestPASS"))
            results.append(scenario_wifi_reconnect(dev_a252))
            # Test coupure/rétablissement WiFi
            results.append(scenario_wifi_cut_restore(dev_a252, "TestSSID", "TestPASS"))
            results.append(scenario_bt_status(dev_a252))
            results.append(scenario_ble_scan(dev_a252))
            # Test explicite de connexion BLE (nom à adapter)
            results.append(scenario_ble_connect(dev_a252, "TestBLEDevice"))

            # Test de coupure WiFi + fallback BLE
            results.append(scenario_wifi_cut_fallback_ble(dev_a252))

            results.append(scenario_wifi_status(dev_s3))
            results.append(scenario_wifi_scan(dev_s3))
            results.append(scenario_wifi_connect(dev_s3, "TestSSID", "TestPASS"))
            results.append(scenario_wifi_reconnect(dev_s3))
            results.append(scenario_wifi_cut_restore(dev_s3, "TestSSID", "TestPASS"))
            results.append(scenario_bt_status(dev_s3))
            results.append(scenario_ble_scan(dev_s3))
            results.append(scenario_ble_connect(dev_s3, "TestBLEDevice"))

            results.append(scenario_wifi_cut_fallback_ble(dev_s3))

            results.append(scenario_slic_transition("SLIC transition A252", dev_a252, bench, "A252"))
            results.append(scenario_slic_transition("SLIC transition S3", dev_s3, bench, "S3"))
            results.append(scenario_a252_full_duplex(dev_a252, bench))
            results.append(scenario_s3_local(dev_s3, bench))
            results.append(scenario_web_access(args.a252_base_url or None, "A252"))
            results.append(scenario_web_access(args.s3_base_url or None, "S3"))

            # --- Sécurisation API ---
            if args.a252_base_url:
                results.append(scenario_api_security(args.a252_base_url))
            if args.s3_base_url:
                results.append(scenario_api_security(args.s3_base_url))
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
