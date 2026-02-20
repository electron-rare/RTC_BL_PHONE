#!/usr/bin/env python3
"""Unit tests for check_web_route_parity parser helpers."""

from __future__ import annotations

import unittest

from scripts.check_web_route_parity import parse_frontend_routes


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


if __name__ == "__main__":
    unittest.main()
