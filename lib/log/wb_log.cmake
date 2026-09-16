# SPDX-License-Identifier: MIT
# Portable wb_log sources for firmware and host unit tests.

set(WB_LOG_ROOT "${CMAKE_CURRENT_LIST_DIR}")

include(${WB_LOG_ROOT}/../time/wb_time.cmake)

set(WB_LOG_SOURCES
  ${WB_LOG_ROOT}/src/wb_log.c
  ${WB_LOG_ROOT}/src/wb_log_stdio.c
  ${WB_TIME_SOURCES}
)

set(WB_LOG_TEST_SOURCES
  ${WB_LOG_SOURCES}
  ${WB_LOG_ROOT}/src/wb_log_stub.c
)

set(WB_LOG_INCLUDE_DIRS
  ${WB_LOG_ROOT}/include
  ${WB_TIME_INCLUDE_DIRS}
)
