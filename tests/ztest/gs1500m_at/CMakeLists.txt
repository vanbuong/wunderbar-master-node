# SPDX-License-Identifier: Apache-2.0
#
# Zephyr ztest suite for GS1500M AT parser.
#   west build -b unit_testing tests/ztest/gs1500m_at -t run

cmake_minimum_required(VERSION 3.20.0)

set(WB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include(${WB_ROOT}/lib/wifi/gs1500m/gs1500m.cmake)

set(GS_AT_SOURCES
  ${GS1500M_ROOT}/src/at_parser.c
  ${GS1500M_ROOT}/src/platform.c
)

if(BOARD STREQUAL "unit_testing" OR BOARD STREQUAL "unit_testing/unit_testing")
  find_package(Zephyr COMPONENTS unittest REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(gs1500m_at_ztest)

  target_include_directories(testbinary PRIVATE ${GS1500M_INCLUDE_DIRS})
  target_sources(testbinary PRIVATE
    src/main.c
    ${GS_AT_SOURCES}
  )
else()
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
  project(gs1500m_at_ztest)

  target_include_directories(app PRIVATE ${GS1500M_INCLUDE_DIRS})
  target_sources(app PRIVATE
    src/main.c
    ${GS_AT_SOURCES}
  )
endif()
