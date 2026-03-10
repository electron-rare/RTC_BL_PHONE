#!/usr/bin/env bash
set -euo pipefail

status="$(git submodule status --recursive || true)"
if [[ -z "${status}" ]]; then
  echo "No submodules found."
  exit 0
fi

bad=0
while IFS= read -r line; do
  prefix="${line:0:1}"
  if [[ "${prefix}" != " " ]]; then
    echo "Submodule drift detected: ${line}" >&2
    bad=1
  fi
done <<< "${status}"

if [[ "${bad}" -ne 0 ]]; then
  echo "Submodule pointers are not clean/initialized. Lock SHAs before commit." >&2
  exit 2
fi

echo "Submodule pointers clean."
