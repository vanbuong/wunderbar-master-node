# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wunderbar_zephyr_wifi)

set(WB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include(${WB_ROOT}/lib/log/wb_log.cmake)
set(GS1500M_PORT zephyr)
include(${WB_ROOT}/lib/wifi/gs1500m/gs1500m.cmake)

target_include_directories(app PRIVATE
  ${WB_LOG_INCLUDE_DIRS}
  ${GS1500M_INCLUDE_DIRS}
  ${GS1500M_PORT_INCLUDE_DIRS}
)

target_sources(app PRIVATE
  src/main.c
  ${WB_LOG_SOURCES}
  ${GS1500M_SOURCES}
  ${GS1500M_PORT_SOURCES}
)
