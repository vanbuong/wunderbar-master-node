# SPDX-License-Identifier: MIT
# Portable system-time module (host/FreeRTOS) or Zephyr SYS_CLOCK_REALTIME.

set(WB_TIME_ROOT "${CMAKE_CURRENT_LIST_DIR}")

if(DEFINED ZEPHYR_BASE OR (DEFINED WB_TIME_BACKEND AND WB_TIME_BACKEND STREQUAL "zephyr"))
  set(WB_TIME_SOURCES
    ${WB_TIME_ROOT}/src/wb_time_zephyr.c
  )
else()
  set(WB_TIME_SOURCES
    ${WB_TIME_ROOT}/src/wb_time.c
  )
endif()

set(WB_TIME_INCLUDE_DIRS
  ${WB_TIME_ROOT}/include
)
