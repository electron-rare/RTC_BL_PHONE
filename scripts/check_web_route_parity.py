#!/usr/bin/env python3
"""Check parity between backend API routes and WebUI API calls."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterable


Route = tuple[str, str]

BACKEND_ROUTE_RE = re.compile(
    r'server_\.on\(\s*"(?P<path>/api/[^"]+)"\s*,\s*HTTP_(?P<method>[A-Z]+)'
)
FRONTEND_CALL_RE = re.compile(
    r'requestJson\(\s*["\'](?P<path>/api/[^"\']+)["\'](?P<args>.*?)\);',
    re.DOTALL,
)
METHOD_RE = re.compile(r'method\s*:\s*["\'](?P<method>[A-Z]+)["\']')


def parse_backend_routes(source: str) -> set[Route]:
    routes: set[Route] = set()
    for match in BACKEND_ROUTE_RE.finditer(source):
        method = match.group("method").upper()
        path = match.group("path")
        routes.add((method, path))
    return routes


def parse_frontend_routes(source: str) -> set[Route]:
    routes: set[Route] = set()
    for match in FRONTEND_CALL_RE.finditer(source):
        path = match.group("path")
        args = match.group("args")
        method_match = METHOD_RE.search(args)
        method = method_match.group("method").upper() if method_match else "GET"
        routes.add((method, path))
    return routes


def format_routes(routes: Iterable[Route]) -> str:
    ordered = sorted(routes, key=lambda route: (route[1], route[0]))
    return "\n".join(f"  - {method} {path}" for method, path in ordered)


def load_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        print(f"[route-parity] missing file: {path}", file=sys.stderr)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate that every WebUI API call is backed by a firmware HTTP route."
    )
    parser.add_argument(
        "--backend",
        default="src/web/WebServerManager.cpp",
        help="Path to backend route source file.",
    )
    parser.add_argument(
        "--frontend",
        default="data/webui/script.js",
        help="Path to frontend source file.",
    )
    parser.add_argument(
        "--strict-unused-backend",
        action="store_true",
        help="Fail if backend API routes are not used by the WebUI.",
    )
    args = parser.parse_args()

    backend_path = Path(args.backend)
    frontend_path = Path(args.frontend)
    backend_source = load_text(backend_path)
    frontend_source = load_text(frontend_path)

    backend_routes = parse_backend_routes(backend_source)
    frontend_routes = parse_frontend_routes(frontend_source)

    if not backend_routes:
        print("[route-parity] no backend /api routes detected", file=sys.stderr)
        return 2
    if not frontend_routes:
        print("[route-parity] no frontend /api requestJson() calls detected", file=sys.stderr)
        return 2

    missing_in_backend = frontend_routes - backend_routes
    unused_backend = backend_routes - frontend_routes

    print(
        f"[route-parity] backend routes: {len(backend_routes)} | frontend routes: {len(frontend_routes)}"
    )

    if missing_in_backend:
        print("[route-parity] missing backend routes for frontend calls:", file=sys.stderr)
        print(format_routes(missing_in_backend), file=sys.stderr)
        return 1

    if unused_backend:
        message = "[route-parity] backend routes currently unused by WebUI:"
        output = format_routes(unused_backend)
        if args.strict_unused_backend:
            print(message, file=sys.stderr)
            print(output, file=sys.stderr)
            return 1
        print(message)
        print(output)

    print("[route-parity] parity check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
