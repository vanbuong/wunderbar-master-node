# SPDX-License-Identifier: Apache-2.0
#
# Zephyr ztest suite for wb_log.
# Host unit test (no board):
#   west build -b unit_testing tests/ztest/wb_log -t run
# Or native_sim:
#   west build -b native_sim tests/ztest/wb_log -t run

cmake_minimum_required(VERSION 3.20.0)

set(WB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include(${WB_ROOT}/lib/log/wb_log.cmake)

if(BOARD STREQUAL "unit_testing" OR BOARD STREQUAL "unit_testing/unit_testing")
  find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(wb_log_ztest)

  target_include_directories(testbinary PRIVATE ${WB_LOG_INCLUDE_DIRS})
  target_sources(testbinary PRIVATE
    src/main.c
    ${WB_LOG_TEST_SOURCES}
  )
else()
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(wb_log_ztest)

  target_include_directories(app PRIVATE ${WB_LOG_INCLUDE_DIRS})
  target_sources(app PRIVATE
    src/main.c
    ${WB_LOG_TEST_SOURCES}
  )
endif()
