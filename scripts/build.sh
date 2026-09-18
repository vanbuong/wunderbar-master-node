#!/usr/bin/env bash
# Build WunderBar master firmware for Zephyr, FreeRTOS, or both.
# Blinky: USB CDC + SEGGER RTT images per OS.
# WiFi: GS1500M demos (USB + RTT) per OS.
# Unit tests: Unity (host) + Zephyr ztest (unit_testing).
#
#   ./scripts/build.sh              # blinky all four images
#   ./scripts/build.sh zephyr
#   ./scripts/build.sh freertos
#   ./scripts/build.sh wifi         # WiFi images (both OS × USB/RTT)
#   ./scripts/build.sh zephyr-wifi
#   ./scripts/build.sh freertos-wifi
#   ./scripts/build.sh test         # Unity + ztest (wb_log + gs1500m AT)
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

ensure_armgcc_sdk() {
  command -v cmake >/dev/null || die "cmake not found"
  command -v ninja >/dev/null || die "ninja not found"
  command -v arm-none-eabi-gcc >/dev/null || die "arm-none-eabi-gcc not found"

  local sdk="${MCU_SDK_PATH:-$ROOT/.deps/mcux-sdk}"
  if [[ ! -d "$sdk/devices/MK64F12" ]]; then
    echo "MCU_SDK_PATH not set / incomplete; fetching GitHub SDK into $sdk"
    "$ROOT/scripts/fetch_mcux_sdk.sh" "$sdk"
  fi
  # Ensure UART driver present (WiFi apps).
  if [[ ! -f "$sdk/drivers/uart/fsl_uart.c" ]] && \
     [[ ! -f "$sdk/devices/MK64F12/drivers/fsl_uart.c" ]]; then
    echo "Fetching UART driver into SDK tree..."
    "$ROOT/scripts/fetch_mcux_sdk.sh" "$sdk"
  fi
  MCU_SDK_PATH="$sdk"
  export MCU_SDK_PATH
}

build_zephyr_app() {
  local app_rel="$1"
  local out_usb="$2"
  local out_rtt="$3"
  local app="$ROOT/$app_rel"

  echo "Building Zephyr $app_rel (USB CDC) -> $out_usb"
  west build -b wunderbar_master/mk64f12 "$app" -d "$out_usb" -- -DBOARD_ROOT="$ROOT"
  echo "Zephyr USB image: $out_usb/zephyr/zephyr.elf"

  echo "Building Zephyr $app_rel (SEGGER RTT) -> $out_rtt"
  west build -b wunderbar_master/mk64f12 "$app" -d "$out_rtt" -- \
    -DBOARD_ROOT="$ROOT" \
    -DEXTRA_CONF_FILE=rtt.conf \
    -DEXTRA_DTC_OVERLAY_FILE=rtt.overlay
  echo "Zephyr RTT image: $out_rtt/zephyr/zephyr.elf"
}

build_zephyr() {
  command -v west >/dev/null || die "west not found (pip install west)"

  if [[ -d /opt/zephyr-sdk-1.0.1 ]]; then
    export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-/opt/zephyr-sdk-1.0.1}"
    export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-zephyr}"
  fi

  ensure_west_workspace
  build_zephyr_app apps/zephyr_blinky \
    "${ZEPHYR_BUILD_DIR:-$ROOT/build-zephyr}" \
    "${ZEPHYR_RTT_BUILD_DIR:-$ROOT/build-zephyr-rtt}"
}

build_zephyr_wifi() {
  command -v west >/dev/null || die "west not found (pip install west)"

  if [[ -d /opt/zephyr-sdk-1.0.1 ]]; then
    export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-/opt/zephyr-sdk-1.0.1}"
    export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-zephyr}"
  fi

  ensure_west_workspace
  build_zephyr_app apps/zephyr_wifi \
    "${ZEPHYR_WIFI_BUILD_DIR:-$ROOT/build-zephyr-wifi}" \
    "${ZEPHYR_WIFI_RTT_BUILD_DIR:-$ROOT/build-zephyr-wifi-rtt}"
}

build_freertos_app() {
  local app_dir="$1"
  local out_usb="$2"
  local out_rtt="$3"
  local elf_name="$4"

  echo "Building FreeRTOS $app_dir (USB CDC) -> $out_usb"
  cmake -S "$ROOT/$app_dir" -B "$out_usb" -G Ninja \
    -DMCU_SDK_PATH="$MCU_SDK_PATH" \
    -DLOG_BACKEND=USB \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
  cmake --build "$out_usb" --parallel "$JOBS"
  echo "FreeRTOS USB image: $out_usb/$elf_name.elf"

  echo "Building FreeRTOS $app_dir (SEGGER RTT) -> $out_rtt"
  cmake -S "$ROOT/$app_dir" -B "$out_rtt" -G Ninja \
    -DMCU_SDK_PATH="$MCU_SDK_PATH" \
    -DLOG_BACKEND=RTT \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
  cmake --build "$out_rtt" --parallel "$JOBS"
  echo "FreeRTOS RTT image: $out_rtt/$elf_name.elf"
}

build_freertos() {
  ensure_armgcc_sdk
  build_freertos_app apps/mcux_freertos_blinky \
    "${FREERTOS_BUILD_DIR:-$ROOT/build-freertos}" \
    "${FREERTOS_RTT_BUILD_DIR:-$ROOT/build-freertos-rtt}" \
    wunderbar_freertos_blinky
}

build_freertos_wifi() {
  ensure_armgcc_sdk
  build_freertos_app apps/mcux_freertos_wifi \
    "${FREERTOS_WIFI_BUILD_DIR:-$ROOT/build-freertos-wifi}" \
    "${FREERTOS_WIFI_RTT_BUILD_DIR:-$ROOT/build-freertos-wifi-rtt}" \
    wunderbar_freertos_wifi
}

run_tests() {
  command -v cmake >/dev/null || die "cmake not found"
  command -v ninja >/dev/null || die "ninja not found"
  command -v gcc >/dev/null || die "host gcc not found"
  command -v west >/dev/null || die "west not found (pip install west)"

  local unity="${UNITY_PATH:-$ROOT/.deps/unity}"
  if [[ ! -f "$unity/src/unity.c" ]]; then
    echo "Fetching Unity into $unity"
    "$ROOT/scripts/fetch_unity.sh" "$unity"
  fi

  local out_unity="${UNITY_BUILD_DIR:-$ROOT/build-unity}"
  echo "Building Unity tests (wb_log + gs1500m AT) -> $out_unity"
  cmake -S "$ROOT/tests/unity" -B "$out_unity" -G Ninja -DUNITY_PATH="$unity"
  cmake --build "$out_unity" --parallel "$JOBS"
  ctest --test-dir "$out_unity" --output-on-failure
  echo "Unity tests OK"

  if [[ -d /opt/zephyr-sdk-1.0.1 ]]; then
    export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-/opt/zephyr-sdk-1.0.1}"
    export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-zephyr}"
  fi

  ensure_west_workspace

  local out_ztest="${ZTEST_BUILD_DIR:-$ROOT/build-ztest}"
  echo "Building Zephyr ztest (wb_log) -> $out_ztest"
  west build -b unit_testing "$ROOT/tests/ztest/wb_log" -d "$out_ztest" -t run
  echo "ztest wb_log OK"

  local out_ztest_gs="${ZTEST_GS_BUILD_DIR:-$ROOT/build-ztest-gs1500m}"
  echo "Building Zephyr ztest (gs1500m AT) -> $out_ztest_gs"
  west build -b unit_testing "$ROOT/tests/ztest/gs1500m_at" -d "$out_ztest_gs" -t run
  echo "ztest gs1500m_at OK"

  local out_ztest_wifi="${ZTEST_GS_WIFI_BUILD_DIR:-$ROOT/build-ztest-gs1500m-wifi}"
  echo "Building Zephyr ztest (gs1500m wifi) -> $out_ztest_wifi"
  west build -b unit_testing "$ROOT/tests/ztest/gs1500m_wifi" -d "$out_ztest_wifi" -t run
  echo "ztest gs1500m_wifi OK"
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
  wifi)
    build_zephyr_wifi
    build_freertos_wifi
    ;;
  zephyr-wifi)
    build_zephyr_wifi
    ;;
  freertos-wifi|mcux-wifi)
    build_freertos_wifi
    ;;
  test|tests)
    run_tests
    ;;
  *)
    die "unknown target '$TARGET' (use: all | zephyr | freertos | wifi | zephyr-wifi | freertos-wifi | test)"
    ;;
esac
