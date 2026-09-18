#!/usr/bin/env bash
# Fetch GNU Arm Embedded Toolchain 10.3-2021.10 (GCC 10.3.1).
#
# Usage:
#   ./scripts/fetch_arm_gcc_10_3_1.sh [dest]
# Dest defaults to $PWD/.deps/gcc-arm-none-eabi-10.3-2021.10
# After install, bin/ is on PATH via:
#   export PATH="$DEST/bin:$PATH"

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="10.3-2021.10"
DEST="${1:-$ROOT/.deps/gcc-arm-none-eabi-${VER}}"
CACHE="${DEST%/*}/src"
TAR_NAME="gcc-arm-none-eabi-${VER}-x86_64-linux.tar.bz2"
URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/${VER}/${TAR_NAME}"

mkdir -p "$CACHE"

if [[ -x "$DEST/bin/arm-none-eabi-gcc" ]]; then
  echo "ARM GCC already present: $DEST"
  "$DEST/bin/arm-none-eabi-gcc" --version | head -1
  exit 0
fi

TAR="$CACHE/$TAR_NAME"
if [[ ! -f "$TAR" ]]; then
  echo "Downloading $URL"
  curl -fL --retry 3 --retry-delay 2 -o "$TAR.partial" "$URL"
  mv "$TAR.partial" "$TAR"
fi

TMP="$CACHE/arm_gcc_extract"
rm -rf "$TMP"
mkdir -p "$TMP"
echo "Extracting $TAR"
tar -xjf "$TAR" -C "$TMP"

INNER="$(find "$TMP" -maxdepth 1 -mindepth 1 -type d | head -1)"
mkdir -p "$(dirname "$DEST")"
rm -rf "$DEST"
mv "$INNER" "$DEST"
rm -rf "$TMP"

echo "ARM GCC ready: $DEST"
"$DEST/bin/arm-none-eabi-gcc" --version | head -1
