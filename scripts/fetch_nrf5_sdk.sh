#!/usr/bin/env bash
# Fetch nRF5 SDK 12.1.0 for apps/nrf51_bt_master.
#
# Usage:
#   ./scripts/fetch_nrf5_sdk.sh [dest]
# Dest defaults to $NRF5_SDK_ROOT, then $PWD/.deps/nRF5_SDK_12.1.0

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-${NRF5_SDK_ROOT:-$ROOT/.deps/nRF5_SDK_12.1.0}}"
CACHE="${DEST%/*}/src"
ZIP_NAME="nRF5_SDK_12.1.0_0d23e2a.zip"
URL="https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v12.x.x/${ZIP_NAME}"

mkdir -p "$CACHE"

if [[ -f "$DEST/components/toolchain/system_nrf51.c" ]]; then
  echo "nRF5 SDK already present: $DEST"
  exit 0
fi

ZIP="$CACHE/$ZIP_NAME"
if [[ ! -f "$ZIP" ]]; then
  echo "Downloading $URL"
  curl -fL --retry 3 --retry-delay 2 -o "$ZIP.partial" "$URL"
  mv "$ZIP.partial" "$ZIP"
fi

TMP="$CACHE/nrf5_sdk_extract"
rm -rf "$TMP"
mkdir -p "$TMP"
echo "Extracting $ZIP"
unzip -q "$ZIP" -d "$TMP"

# Nordic ships this zip flat (components/ at root). Older mirrors used a
# single top-level folder — support both.
mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
if [[ -f "$TMP/components/toolchain/system_nrf51.c" ]]; then
  mv "$TMP" "$DEST"
else
  INNER="$(find "$TMP" -maxdepth 1 -mindepth 1 -type d | head -1)"
  if [[ -z "$INNER" || ! -f "$INNER/components/toolchain/system_nrf51.c" ]]; then
    echo "error: unexpected nRF5 SDK zip layout under $TMP" >&2
    ls -la "$TMP" >&2 || true
    exit 1
  fi
  mv "$INNER" "$DEST"
  rm -rf "$TMP"
fi

echo "nRF5 SDK ready: $DEST"
