# SPDX-License-Identifier: MIT
# Host Unity unit tests for wb_log (no MCU / FreeRTOS required).
#
#   ../../scripts/fetch_unity.sh
#   cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build

cmake_minimum_required(VERSION 3.20)
project(wb_log_unity_tests C)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include(${ROOT}/lib/log/wb_log.cmake)

if(NOT UNITY_PATH)
  foreach(_u IN ITEMS
      "${ROOT}/.deps/unity"
      "${ROOT}/.deps/src/unity"
      "$ENV{HOME}/deps/src/unity")
    if(EXISTS "${_u}/src/unity.c")
      set(UNITY_PATH "${_u}")
      break()
    endif()
  endforeach()
endif()
if(NOT UNITY_PATH)
  message(FATAL_ERROR
    "Unity not found. Run ../../scripts/fetch_unity.sh or pass -DUNITY_PATH=")
endif()

add_executable(test_wb_log
  test_wb_log.c
  ${WB_LOG_TEST_SOURCES}
  ${UNITY_PATH}/src/unity.c
)

target_include_directories(test_wb_log PRIVATE
  ${WB_LOG_INCLUDE_DIRS}
  ${UNITY_PATH}/src
)

enable_testing()
add_test(NAME wb_log_unity COMMAND test_wb_log)
