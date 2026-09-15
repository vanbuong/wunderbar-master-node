# SPDX-License-Identifier: Apache-2.0

# Device ID for J-Link / OpenOCD. MK24FN1M0xxx12 is the correct silicon;
# Zephyr SoC support currently builds against the MK64F12 device headers.
board_runner_args(jlink "--device=MK24FN1M0xxx12")
board_runner_args(pyocd "--target=k64f")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
