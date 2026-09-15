#!/usr/bin/env bash
# Build WunderBar master blinky for Zephyr, FreeRTOS, or both.
#
#   ./scripts/build.sh              # both
#   ./scripts/build.sh zephyr
#   ./scripts/build.sh freertos
#
# Zephyr: west init -l requires a sibling workspace whose project directory
# is named wunderbar-master-node. This script creates $WB_WEST_WORKSPACE
# (default: $HOME/wb-zephyr-workspace) if needed.
# ZEPHYR_SDK_INSTALL_DIR must point at a Zephyr SDK with the ARM toolchain,
# or ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb with arm-none-eabi-gcc on PATH.
#
# FreeRTOS: set MCU_SDK_PATH, or let this script fetch GitHub SDK sources
# into .deps/mcux-sdk.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET="${1:-all}"
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc 2>/dev/null || echo 4)}"

die() { echo "error: $*" >&2; exit 1; }

ensure_west_workspace() {
  if west topdir >/dev/null 2>&1; then
    return
  fi

  local ws="${WB_WEST_WORKSPACE:-$HOME/wb-zephyr-workspace}"
  mkdir -p "$ws"

  if [[ ! -d "$ws/.west" ]]; then
    if [[ "$(basename "$ROOT")" == "wunderbar-master-node" ]] && \
       [[ "$(cd "$ROOT/.." && pwd)" == "$(cd "$ws" && pwd)" ]]; then
      (cd "$ws" && west init -l wunderbar-master-node)
    else
      # Checkout directory is not named wunderbar-master-node (common in CI
      # sandboxes). Point the workspace at ROOT via a relative manifest.path.
      mkdir -p "$ws/.west"
      local rel
      rel="$(python3 -c 'import os,sys; print(os.path.relpath(sys.argv[1], sys.argv[2]))' "$ROOT" "$ws")"
      printf '[manifest]\npath = %s\nfile = west.yml\n' "$rel" > "$ws/.west/config"
      echo "Created west workspace $ws -> $rel"
    fi
  fi

  if [[ ! -d "$ws/zephyr" ]]; then
    echo "Updating west projects (this can take a while on first run)..."
    (cd "$ws" && west update --narrow -o=--depth=1)
  fi

  cd "$ws"
}

build_zephyr() {
  command -v west >/dev/null || die "west not found (pip install west)"

  if [[ -d /opt/zephyr-sdk-1.0.1 ]]; then
    export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-/opt/zephyr-sdk-1.0.1}"
    export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-zephyr}"
  fi

  ensure_west_workspace

  local app="$ROOT/apps/zephyr_blinky"
  local out="${ZEPHYR_BUILD_DIR:-$ROOT/build-zephyr}"
  echo "Building Zephyr blinky -> $out"
  west build -b wunderbar_master/mk64f12 "$app" -d "$out" -- -DBOARD_ROOT="$ROOT"
  echo "Zephyr image: $out/zephyr/zephyr.elf"
}

build_freertos() {
  command -v cmake >/dev/null || die "cmake not found"
  command -v ninja >/dev/null || die "ninja not found"
  command -v arm-none-eabi-gcc >/dev/null || die "arm-none-eabi-gcc not found"

  local sdk="${MCU_SDK_PATH:-$ROOT/.deps/mcux-sdk}"
  if [[ ! -d "$sdk/devices/MK64F12" ]]; then
    echo "MCU_SDK_PATH not set / incomplete; fetching GitHub SDK into $sdk"
    "$ROOT/scripts/fetch_mcux_sdk.sh" "$sdk"
  fi

  local out="${FREERTOS_BUILD_DIR:-$ROOT/build-freertos}"
  echo "Building FreeRTOS blinky -> $out"
  cmake -S "$ROOT/apps/mcux_freertos_blinky" -B "$out" -G Ninja \
    -DMCU_SDK_PATH="$sdk" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
  cmake --build "$out" --parallel "$JOBS"
  echo "FreeRTOS image: $out/wunderbar_freertos_blinky.elf"
}

case "$TARGET" in
  all|both)
    build_zephyr
    build_freertos
    ;;
  zephyr)
    build_zephyr
    ;;
  freertos|mcux)
    build_freertos
    ;;
  *)
    die "unknown target '$TARGET' (use: all | zephyr | freertos)"
    ;;
esac
