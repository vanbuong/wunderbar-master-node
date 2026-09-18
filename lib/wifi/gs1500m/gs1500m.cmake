# SPDX-License-Identifier: MIT
# Portable GS1500M Serial2WiFi library sources.
#
#   include(${ROOT}/lib/wifi/gs1500m/gs1500m.cmake)
#   target_sources(... ${GS1500M_SOURCES})
#   target_include_directories(... ${GS1500M_INCLUDE_DIRS})
#
# Optional ports:
#   GS1500M_PORT=freertos  → also ${GS1500M_PORT_SOURCES}
#   GS1500M_PORT=zephyr
#   GS1500M_PORT=none|host → core only (unit tests)

set(GS1500M_ROOT "${CMAKE_CURRENT_LIST_DIR}")

include(${GS1500M_ROOT}/../../time/wb_time.cmake)

set(GS1500M_INCLUDE_DIRS
  ${GS1500M_ROOT}/include
  ${WB_TIME_INCLUDE_DIRS}
)

set(GS1500M_SOURCES
  ${GS1500M_ROOT}/src/platform.c
  ${GS1500M_ROOT}/src/at_parser.c
  ${GS1500M_ROOT}/src/at_cmd.c
  ${GS1500M_ROOT}/src/wifi.c
  ${GS1500M_ROOT}/src/socket.c
  ${GS1500M_ROOT}/src/ssl.c
  ${GS1500M_ROOT}/src/http.c
  ${GS1500M_ROOT}/src/mqtt_pipe.c
  ${GS1500M_ROOT}/src/limited_ap.c
  ${GS1500M_ROOT}/src/user.c
)

set(GS1500M_PORT_SOURCES "")
set(GS1500M_PORT_INCLUDE_DIRS "")

if(NOT DEFINED GS1500M_PORT)
  set(GS1500M_PORT "none")
endif()

string(TOLOWER "${GS1500M_PORT}" GS1500M_PORT_LOWER)

if(GS1500M_PORT_LOWER STREQUAL "freertos")
  list(APPEND GS1500M_PORT_SOURCES
    ${GS1500M_ROOT}/port/freertos_mcux/gs_platform_freertos.c)
  list(APPEND GS1500M_PORT_INCLUDE_DIRS
    ${GS1500M_ROOT}/port/freertos_mcux)
elseif(GS1500M_PORT_LOWER STREQUAL "zephyr")
  list(APPEND GS1500M_PORT_SOURCES
    ${GS1500M_ROOT}/port/zephyr/gs_platform_zephyr.c)
  list(APPEND GS1500M_PORT_INCLUDE_DIRS
    ${GS1500M_ROOT}/port/zephyr)
endif()
