# SPDX-License-Identifier: Apache-2.0
#
# Zephyr ztest suite for GS1500M WiFi library (stub platform).
#   west build -b unit_testing tests/ztest/gs1500m_wifi -t run

cmake_minimum_required(VERSION 3.20.0)

set(WB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(GS1500M_PORT none)
include(${WB_ROOT}/lib/wifi/gs1500m/gs1500m.cmake)

set(GS_WIFI_TEST_SOURCES
  src/main.c
  ${WB_ROOT}/tests/common/gs_stub_platform.c
  ${GS1500M_SOURCES}
  ${WB_TIME_SOURCES}
)

if(BOARD STREQUAL "unit_testing" OR BOARD STREQUAL "unit_testing/unit_testing")
  find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(gs1500m_wifi_ztest)

  target_include_directories(testbinary PRIVATE
    ${GS1500M_INCLUDE_DIRS}
    ${WB_TIME_INCLUDE_DIRS}
    ${WB_ROOT}/tests/common
  )
  target_sources(testbinary PRIVATE ${GS_WIFI_TEST_SOURCES})
else()
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(gs1500m_wifi_ztest)

  target_include_directories(app PRIVATE
    ${GS1500M_INCLUDE_DIRS}
    ${WB_TIME_INCLUDE_DIRS}
    ${WB_ROOT}/tests/common
  )
  target_sources(app PRIVATE ${GS_WIFI_TEST_SOURCES})
endif()
