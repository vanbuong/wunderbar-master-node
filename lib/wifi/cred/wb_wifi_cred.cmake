# SPDX-License-Identifier: MIT
# Patchable WiFi credential flash slot.

set(WB_WIFI_CRED_ROOT "${CMAKE_CURRENT_LIST_DIR}")

if(NOT DEFINED WB_WIFI_CRED_FLASH_ADDR)
  set(WB_WIFI_CRED_FLASH_ADDR "0x0007E000")
endif()

set(WB_WIFI_CRED_INCLUDE_DIRS
  ${WB_WIFI_CRED_ROOT}/include
)

set(WB_WIFI_CRED_SOURCES
  ${WB_WIFI_CRED_ROOT}/src/wb_wifi_cred.c
)

# GNU ld: force section to absolute flash address (MK24/K64 flash @ 0x0).
set(WB_WIFI_CRED_LINK_FLAGS
  "-Wl,--section-start=.wb_wifi_cred=${WB_WIFI_CRED_FLASH_ADDR}"
  "-Wl,-u,wb_wifi_cred"
)
