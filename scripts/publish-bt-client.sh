#!/usr/bin/env bash
# Builds the Web Bluetooth page and the esp32dev firmware and copies both into a
# checkout of the letalvoj.github.io repository, where they are served at
# https://letalvoj.github.io/obegraensad-bt-client/. Commit and push that repository
# afterwards.
#
#   scripts/publish-bt-client.sh ../letalvoj.github.io
set -euo pipefail

site=${1:?usage: $0 <path to letalvoj.github.io checkout>}
repo=$(cd "$(dirname "$0")/.." && pwd)
target="$site/obegraensad-bt-client"

(cd "$repo/frontend" && npx -y pnpm@10.27.0 install --frozen-lockfile && npx -y pnpm@10.27.0 build:ble)
(cd "$repo" && pio run --environment esp32dev)

mkdir -p "$target"
cp "$repo/frontend/dist-ble/index.html" "$target/index.html"
cp "$repo/.pio/build/esp32dev/firmware.bin" "$target/firmware-esp32dev.bin"

echo "Updated $target:"
ls -la "$target"
