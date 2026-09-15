#!/usr/bin/env bash
# Assemble an MCU_SDK_PATH tree for apps/mcux_freertos_blinky without the
# MCUXpresso SDK Builder zip. Uses NXP's GitHub SDK 2.x sources + CMSIS +
# FreeRTOS-Kernel.
#
# Usage:
#   ./scripts/fetch_mcux_sdk.sh [dest]
# Dest defaults to $MCU_SDK_PATH, then $PWD/.deps/mcux-sdk

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-${MCU_SDK_PATH:-$ROOT/.deps/mcux-sdk}}"
CACHE="${DEST%/*}/src"

mkdir -p "$CACHE" "$DEST/rtos/freertos"

clone_sparse() {
  local url="$1" dir="$2"
  shift 2
  if [[ ! -d "$dir/.git" ]]; then
    git clone --depth 1 --filter=blob:none --sparse "$url" "$dir"
  fi
  git -C "$dir" sparse-checkout set "$@"
  git -C "$dir" checkout --quiet
}

if [[ ! -d "$CACHE/legacy-mcux-sdk/.git" ]]; then
  git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/nxp-mcuxpresso/legacy-mcux-sdk.git "$CACHE/legacy-mcux-sdk"
fi
git -C "$CACHE/legacy-mcux-sdk" sparse-checkout set \
  devices/MK64F12 \
  drivers/common \
  drivers/gpio \
  drivers/port \
  drivers/smc
git -C "$CACHE/legacy-mcux-sdk" checkout --quiet

if [[ ! -d "$CACHE/CMSIS_5/.git" ]]; then
  git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/ARM-software/CMSIS_5.git "$CACHE/CMSIS_5"
fi
git -C "$CACHE/CMSIS_5" sparse-checkout set CMSIS/Core/Include
git -C "$CACHE/CMSIS_5" checkout --quiet

if [[ ! -d "$CACHE/FreeRTOS-Kernel/.git" ]]; then
  git clone --depth 1 --branch MCUX_2.16.000 \
    https://github.com/nxp-mcuxpresso/FreeRTOS-Kernel.git "$CACHE/FreeRTOS-Kernel" \
    || git clone --depth 1 --branch V11.1.0 \
         https://github.com/FreeRTOS/FreeRTOS-Kernel.git "$CACHE/FreeRTOS-Kernel"
fi

ln -sfn "$CACHE/legacy-mcux-sdk/devices" "$DEST/devices"
ln -sfn "$CACHE/legacy-mcux-sdk/drivers" "$DEST/drivers"
ln -sfn "$CACHE/CMSIS_5/CMSIS" "$DEST/CMSIS"
ln -sfn "$CACHE/FreeRTOS-Kernel" "$DEST/rtos/freertos/freertos-kernel"

echo "MCU_SDK_PATH=$DEST"
test -d "$DEST/devices/MK64F12"
test -f "$DEST/CMSIS/Core/Include/core_cm4.h"
test -f "$DEST/rtos/freertos/freertos-kernel/tasks.c"
echo "SDK fetch OK"
