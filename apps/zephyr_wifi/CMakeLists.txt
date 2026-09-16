# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wunderbar_zephyr_wifi)

set(WB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include(${WB_ROOT}/lib/wifi/cred/wb_wifi_cred.cmake)
# Headers only — wb_time symbols come from the GS1500M driver library.
include(${WB_ROOT}/lib/time/wb_time.cmake)

zephyr_linker_sources(SECTIONS ${CMAKE_CURRENT_SOURCE_DIR}/wb_wifi_cred.ld)

target_include_directories(app PRIVATE
  ${WB_WIFI_CRED_INCLUDE_DIRS}
  ${WB_TIME_INCLUDE_DIRS}
)

target_sources(app PRIVATE
  src/main.c
  ${WB_WIFI_CRED_SOURCES}
)

target_compile_definitions(app PRIVATE
  WB_WIFI_CRED_FLASH_ADDR=${WB_WIFI_CRED_FLASH_ADDR}
)
