#!/usr/bin/env python3
"""Unit tests for check_web_route_parity parser helpers."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from scripts.check_web_route_parity import build_report, parse_frontend_routes, write_report_json


class ParseFrontendRoutesTest(unittest.TestCase):
    def test_detects_direct_calls(self) -> None:
        source = """
        async function refresh() {
          await requestJson("/api/status");
          await requestJson("/api/control", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ action: "CALL" }),
          });
        }
        """
        routes = parse_frontend_routes(source)
        self.assertIn(("GET", "/api/status"), routes)
        self.assertIn(("POST", "/api/control"), routes)

    def test_detects_calls_inside_promise_all(self) -> None:
        source = """
        const [wifi, mqtt, espnow, peers] = await Promise.all([
          requestJson("/api/network/wifi"),
          requestJson("/api/network/mqtt"),
          requestJson("/api/network/espnow"),
          requestJson("/api/network/espnow/peer"),
        ]);
        """
        routes = parse_frontend_routes(source)
        self.assertIn(("GET", "/api/network/wifi"), routes)
        self.assertIn(("GET", "/api/network/mqtt"), routes)
        self.assertIn(("GET", "/api/network/espnow"), routes)
        self.assertIn(("GET", "/api/network/espnow/peer"), routes)

    def test_handles_nested_parentheses_in_payload(self) -> None:
        source = """
        await requestJson("/api/network/mqtt/publish", {
          method: "POST",
          body: JSON.stringify({
            topic: "rtc/test",
            payload: JSON.stringify({ ping: true }),
          }),
        });
        """
        routes = parse_frontend_routes(source)
        self.assertIn(("POST", "/api/network/mqtt/publish"), routes)


class ReportParityJsonTest(unittest.TestCase):
    def test_build_report_has_expected_structure(self) -> None:
        backend_routes = {("GET", "/api/status"), ("POST", "/api/control")}
        frontend_routes = {("GET", "/api/status"), ("POST", "/api/control")}
        report = build_report(
            backend_routes=backend_routes,
            frontend_routes=frontend_routes,
            missing_in_backend=set(),
            unused_backend=set(),
            strict_unused_backend=False,
            status="pass",
        )

        self.assertEqual(report["backend_count"], 2)
        self.assertEqual(report["frontend_count"], 2)
        self.assertEqual(report["status"], "pass")
        self.assertIn("backend_routes", report)
        self.assertIn("frontend_routes", report)
        self.assertIn("missing_in_backend", report)
        self.assertIn("unused_backend", report)
        self.assertFalse(report["strict_unused_backend"])

    def test_build_report_routes_are_stably_sorted(self) -> None:
        backend_routes = {
            ("POST", "/api/network/mqtt/connect"),
            ("GET", "/api/status"),
            ("DELETE", "/api/network/espnow/peer"),
        }
        report = build_report(
            backend_routes=backend_routes,
            frontend_routes=set(),
            missing_in_backend=set(),
            unused_backend=backend_routes,
            strict_unused_backend=True,
            status="fail",
        )

        self.assertEqual(
            report["backend_routes"],
            [
                {"method": "DELETE", "path": "/api/network/espnow/peer"},
                {"method": "POST", "path": "/api/network/mqtt/connect"},
                {"method": "GET", "path": "/api/status"},
            ],
        )

    def test_mismatch_report_marks_fail_and_lists_missing(self) -> None:
        backend_routes = {("GET", "/api/status")}
        frontend_routes = {("GET", "/api/status"), ("POST", "/api/control")}
        missing = frontend_routes - backend_routes
        report = build_report(
            backend_routes=backend_routes,
            frontend_routes=frontend_routes,
            missing_in_backend=missing,
            unused_backend=backend_routes - frontend_routes,
            strict_unused_backend=False,
            status="fail",
        )

        self.assertEqual(report["status"], "fail")
        self.assertEqual(
            report["missing_in_backend"],
            [{"method": "POST", "path": "/api/control"}],
        )

    def test_write_report_json_writes_valid_json(self) -> None:
        report = build_report(
            backend_routes={("GET", "/api/status")},
            frontend_routes={("GET", "/api/status")},
            missing_in_backend=set(),
            unused_backend=set(),
            strict_unused_backend=False,
            status="pass",
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "route_parity_report.json"
            write_report_json(path, report)
            loaded = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["status"], "pass")
            self.assertEqual(loaded["backend_count"], 1)


if __name__ == "__main__":
    unittest.main()
