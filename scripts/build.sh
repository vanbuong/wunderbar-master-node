#!/usr/bin/env bash
# Build WunderBar master blinky for Zephyr, FreeRTOS, or both.
# Each OS produces USB CDC and SEGGER RTT images (four ELFs for "all").
#
#   ./scripts/build.sh              # all four
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
  local out_usb="${ZEPHYR_BUILD_DIR:-$ROOT/build-zephyr}"
  local out_rtt="${ZEPHYR_RTT_BUILD_DIR:-$ROOT/build-zephyr-rtt}"

  echo "Building Zephyr blinky (USB CDC) -> $out_usb"
  west build -b wunderbar_master/mk64f12 "$app" -d "$out_usb" -- -DBOARD_ROOT="$ROOT"
  echo "Zephyr USB image: $out_usb/zephyr/zephyr.elf"

  echo "Building Zephyr blinky (SEGGER RTT) -> $out_rtt"
  west build -b wunderbar_master/mk64f12 "$app" -d "$out_rtt" -- \
    -DBOARD_ROOT="$ROOT" \
    -DEXTRA_CONF_FILE=rtt.conf \
    -DEXTRA_DTC_OVERLAY_FILE=rtt.overlay
  echo "Zephyr RTT image: $out_rtt/zephyr/zephyr.elf"
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

  local out_usb="${FREERTOS_BUILD_DIR:-$ROOT/build-freertos}"
  local out_rtt="${FREERTOS_RTT_BUILD_DIR:-$ROOT/build-freertos-rtt}"

  echo "Building FreeRTOS blinky (USB CDC) -> $out_usb"
  cmake -S "$ROOT/apps/mcux_freertos_blinky" -B "$out_usb" -G Ninja \
    -DMCU_SDK_PATH="$sdk" \
    -DLOG_BACKEND=USB \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
  cmake --build "$out_usb" --parallel "$JOBS"
  echo "FreeRTOS USB image: $out_usb/wunderbar_freertos_blinky.elf"

  echo "Building FreeRTOS blinky (SEGGER RTT) -> $out_rtt"
  cmake -S "$ROOT/apps/mcux_freertos_blinky" -B "$out_rtt" -G Ninja \
    -DMCU_SDK_PATH="$sdk" \
    -DLOG_BACKEND=RTT \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
  cmake --build "$out_rtt" --parallel "$JOBS"
  echo "FreeRTOS RTT image: $out_rtt/wunderbar_freertos_blinky.elf"
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
