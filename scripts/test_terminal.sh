#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "[test_terminal] build unit tests (no upload / no hardware)"
platformio test --without-uploading --without-testing -e esp32dev

echo "[test_terminal] run host dtmf tests"
mkdir -p .pio/host
c++ -std=c++17 -Wall -Wextra -pedantic -Isrc test/host/test_dtmf_host.cpp src/telephony/DtmfDecoder.cpp -o .pio/host/test_dtmf_host
.pio/host/test_dtmf_host

echo "[test_terminal] all checks passed"
