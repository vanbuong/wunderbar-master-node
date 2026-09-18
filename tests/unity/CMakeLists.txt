# SPDX-License-Identifier: MIT
# Host Unity unit tests for wb_log + gs1500m AT parser + WiFi library.
#
#   ../../scripts/fetch_unity.sh
#   cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build

cmake_minimum_required(VERSION 3.20)
project(wb_unity_tests C)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
include(${ROOT}/lib/log/wb_log.cmake)
set(GS1500M_PORT none)
include(${ROOT}/lib/wifi/gs1500m/gs1500m.cmake)

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

add_executable(test_wb_time
  test_wb_time.c
  ${WB_TIME_SOURCES}
  ${UNITY_PATH}/src/unity.c
)
target_include_directories(test_wb_time PRIVATE
  ${WB_TIME_INCLUDE_DIRS}
  ${UNITY_PATH}/src
)

add_executable(test_gs1500m_at
  test_gs1500m_at.c
  ${GS1500M_ROOT}/src/at_parser.c
  ${GS1500M_ROOT}/src/platform.c
  ${UNITY_PATH}/src/unity.c
)
target_include_directories(test_gs1500m_at PRIVATE
  ${GS1500M_INCLUDE_DIRS}
  ${UNITY_PATH}/src
)

add_executable(test_gs1500m_wifi
  test_gs1500m_wifi.c
  ${ROOT}/tests/common/gs_stub_platform.c
  ${GS1500M_SOURCES}
  ${WB_TIME_SOURCES}
  ${UNITY_PATH}/src/unity.c
)
target_include_directories(test_gs1500m_wifi PRIVATE
  ${ROOT}/tests/common
  ${GS1500M_INCLUDE_DIRS}
  ${WB_TIME_INCLUDE_DIRS}
  ${UNITY_PATH}/src
)

add_executable(test_wb_bt_frame
  test_wb_bt_frame.c
  ${UNITY_PATH}/src/unity.c
)
target_include_directories(test_wb_bt_frame PRIVATE
  ${ROOT}/lib/bt/include
  ${UNITY_PATH}/src
)

enable_testing()
add_test(NAME wb_log_unity COMMAND test_wb_log)
add_test(NAME wb_time_unity COMMAND test_wb_time)
add_test(NAME gs1500m_at_unity COMMAND test_gs1500m_at)
add_test(NAME gs1500m_wifi_unity COMMAND test_gs1500m_wifi)
add_test(NAME wb_bt_frame_unity COMMAND test_wb_bt_frame)
