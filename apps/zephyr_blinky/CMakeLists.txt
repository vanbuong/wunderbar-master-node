# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wunderbar_zephyr_blinky)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/log/wb_log.cmake)

target_include_directories(app PRIVATE ${WB_LOG_INCLUDE_DIRS})
target_sources(app PRIVATE
  src/main.c
  ${WB_LOG_SOURCES}
)
