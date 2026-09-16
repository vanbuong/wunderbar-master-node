# SPDX-License-Identifier: Apache-2.0

zephyr_library()

set(WB_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
set(GS1500M_PORT zephyr)
include(${WB_ROOT}/lib/wifi/gs1500m/gs1500m.cmake)

zephyr_library_sources(
  gs.c
  gs_offload_stub.c
  ${GS1500M_SOURCES}
  ${GS1500M_PORT_SOURCES}
  ${WB_TIME_SOURCES}
)

zephyr_library_include_directories(
  ${GS1500M_INCLUDE_DIRS}
  ${GS1500M_PORT_INCLUDE_DIRS}
  ${WB_TIME_INCLUDE_DIRS}
)
