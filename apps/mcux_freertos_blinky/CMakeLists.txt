# WunderBar master — MCUXpresso SDK + FreeRTOS blinky (PTA29) + USB CDC
#
# Prerequisites
#   1. MCUXpresso SDK with devices/MK64F12, either:
#        a) FRDM-K64F ARM-GCC zip from https://mcuxpresso.nxp.com/en/builder
#           (include FreeRTOS), or
#        b) GitHub sources via ../../scripts/fetch_mcux_sdk.sh
#   2. TinyUSB 0.17.x and SEGGER RTT (fetched by fetch_mcux_sdk.sh).
#   3. arm-none-eabi-gcc on PATH (or set TOOLCHAIN_PREFIX).
#   4. CMake >= 3.20 and Ninja (or MinGW Makefiles).
#
# Build
#   ../../scripts/build.sh freertos
#   # USB CDC (default):
#   cmake -S . -B build -G Ninja -DMCU_SDK_PATH=... -DLOG_BACKEND=USB
#   # SEGGER RTT:
#   cmake -S . -B build-rtt -G Ninja -DMCU_SDK_PATH=... -DLOG_BACKEND=RTT
#
# Flash (example with J-Link)
#   JLinkExe -device MK24FN1M0xxx12 -if SWD -speed 4000
#   # then: loadfile build/wunderbar_freertos_blinky.elf

cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT DEFINED TOOLCHAIN_PREFIX)
  set(TOOLCHAIN_PREFIX arm-none-eabi-)
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)

project(wunderbar_freertos_blinky C ASM)

if(NOT MCU_SDK_PATH)
  message(FATAL_ERROR
    "Set -DMCU_SDK_PATH= to your MCUXpresso SDK root "
    "(FRDM-K64F package that contains devices/MK64F12).")
endif()

# Prefer MK64F12 (current SDK layout). Fall back to MK24F12 if present.
# GitHub legacy-mcux-sdk keeps the device tree at devices/MK64F12; exported
# FRDM-K64F zips use the same path.
if(EXISTS "${MCU_SDK_PATH}/devices/MK64F12")
  set(MCU_DEVICE_DIR "${MCU_SDK_PATH}/devices/MK64F12")
  set(MCU_DEVICE     MK64F12)
  set(MCU_CPU_DEFINE CPU_MK64FN1M0VDC12)
elseif(EXISTS "${MCU_SDK_PATH}/devices/MK24F12")
  set(MCU_DEVICE_DIR "${MCU_SDK_PATH}/devices/MK24F12")
  set(MCU_DEVICE     MK24F12)
  set(MCU_CPU_DEFINE CPU_MK24FN1M0VDC12)
else()
  message(FATAL_ERROR "Neither devices/MK64F12 nor devices/MK24F12 found under MCU_SDK_PATH")
endif()

# CMSIS-Core headers: exported SDK zip, ARM CMSIS_5 overlay, or NXP west layout.
if(NOT CMSIS_INCLUDE)
  foreach(_cmsis_inc IN ITEMS
      "${MCU_SDK_PATH}/CMSIS/Core/Include"
      "${MCU_SDK_PATH}/core/CMSIS/CMSIS/Core/Include"
      "${MCU_SDK_PATH}/CMSIS/Include")
    if(EXISTS "${_cmsis_inc}/core_cm4.h")
      set(CMSIS_INCLUDE "${_cmsis_inc}")
      break()
    endif()
  endforeach()
endif()
if(NOT CMSIS_INCLUDE)
  message(FATAL_ERROR
    "CMSIS Core headers not found. Place CMSIS/Core/Include under MCU_SDK_PATH "
    "or pass -DCMSIS_INCLUDE=/path/to/CMSIS/Core/Include")
endif()

set(MCU_LD "${MCU_DEVICE_DIR}/gcc/${MCU_DEVICE}N1M0xxx12_flash.ld")
if(NOT EXISTS "${MCU_LD}")
  # SDK naming uses MK64FN1M0xxx12_flash.ld
  file(GLOB MCU_LD "${MCU_DEVICE_DIR}/gcc/*flash.ld")
  list(GET MCU_LD 0 MCU_LD)
endif()

# FreeRTOS: SDK middleware path (exported with FreeRTOS component) or override.
if(NOT FREERTOS_PATH)
  if(EXISTS "${MCU_SDK_PATH}/rtos/freertos/freertos-kernel")
    set(FREERTOS_PATH "${MCU_SDK_PATH}/rtos/freertos/freertos-kernel")
  elseif(EXISTS "${MCU_SDK_PATH}/rtos/freertos/FreeRTOS")
    set(FREERTOS_PATH "${MCU_SDK_PATH}/rtos/freertos/FreeRTOS")
  elseif(EXISTS "${MCU_SDK_PATH}/rtos/amazon-freertos/freertos_kernel")
    set(FREERTOS_PATH "${MCU_SDK_PATH}/rtos/amazon-freertos/freertos_kernel")
  else()
    message(FATAL_ERROR
      "FreeRTOS kernel not found. Re-export the SDK with FreeRTOS, "
      "or pass -DFREERTOS_PATH= to the kernel root.")
  endif()
endif()

set(CPU_FLAGS
  -mcpu=cortex-m4
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
)

add_compile_options(
  ${CPU_FLAGS}
  -ffunction-sections
  -fdata-sections
  -Wall
  -fno-common
  -ffreestanding
  --specs=nano.specs
)

add_compile_definitions(
  ${MCU_CPU_DEFINE}
  CPU_${MCU_DEVICE}
  __STARTUP_CLEAR_BSS
  SDK_DEBUGCONSOLE=0
)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/board
  ${MCU_DEVICE_DIR}
  ${MCU_DEVICE_DIR}/drivers
  ${CMSIS_INCLUDE}
  ${MCU_SDK_PATH}/devices/${MCU_DEVICE}/cmsis_drivers
  ${MCU_SDK_PATH}/drivers/common
  ${MCU_SDK_PATH}/drivers/gpio
  ${MCU_SDK_PATH}/drivers/port
  ${MCU_SDK_PATH}/drivers/smc
  ${FREERTOS_PATH}/include
  ${FREERTOS_PATH}/portable/GCC/ARM_CM4F
)

# Device startup + system
file(GLOB DEVICE_STARTUP "${MCU_DEVICE_DIR}/gcc/startup_*.S")
set(DEVICE_SYSTEM "${MCU_DEVICE_DIR}/system_${MCU_DEVICE}.c")

# Minimal drivers for blinky + clock.
# Exported FRDM-K64F zips keep these under devices/<part>/drivers/;
# GitHub legacy-mcux-sdk keeps shared drivers under drivers/<ip>/.
set(_driver_candidates
  ${MCU_DEVICE_DIR}/drivers/fsl_clock.c
  ${MCU_DEVICE_DIR}/drivers/fsl_gpio.c
  ${MCU_DEVICE_DIR}/drivers/fsl_port.c
  ${MCU_DEVICE_DIR}/drivers/fsl_smc.c
  ${MCU_DEVICE_DIR}/drivers/fsl_common.c
  ${MCU_DEVICE_DIR}/drivers/fsl_common_arm.c
  ${MCU_SDK_PATH}/drivers/gpio/fsl_gpio.c
  ${MCU_SDK_PATH}/drivers/port/fsl_port.c
  ${MCU_SDK_PATH}/drivers/smc/fsl_smc.c
  ${MCU_SDK_PATH}/drivers/common/fsl_common.c
  ${MCU_SDK_PATH}/drivers/common/fsl_common_arm.c
)

set(SDK_DRIVERS)
foreach(_f IN LISTS _driver_candidates)
  if(EXISTS "${_f}")
    list(APPEND SDK_DRIVERS "${_f}")
  endif()
endforeach()
list(REMOVE_DUPLICATES SDK_DRIVERS)

set(FREERTOS_SOURCES
  ${FREERTOS_PATH}/tasks.c
  ${FREERTOS_PATH}/queue.c
  ${FREERTOS_PATH}/list.c
  ${FREERTOS_PATH}/timers.c
  ${FREERTOS_PATH}/event_groups.c
  ${FREERTOS_PATH}/stream_buffer.c
  ${FREERTOS_PATH}/portable/MemMang/heap_4.c
  ${FREERTOS_PATH}/portable/GCC/ARM_CM4F/port.c
)

set(LOG_BACKEND "USB" CACHE STRING "Log backend: USB (CDC ACM) or RTT (SEGGER J-Link)")
string(TOUPPER "${LOG_BACKEND}" LOG_BACKEND_UPPER)

set(APP_SOURCES
  src/main.c
  src/syscalls.c
  board/clock_config.c
  board/pin_mux.c
  board/board.c
)
set(LOG_SOURCES)

if(LOG_BACKEND_UPPER STREQUAL "RTT")
  add_compile_definitions(LOG_BACKEND_RTT=1)
  if(NOT SEGGER_RTT_PATH)
    foreach(_rtt IN ITEMS
        "${MCU_SDK_PATH}/middleware/segger-rtt"
        "${MCU_SDK_PATH}/../src/segger-rtt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../.deps/src/segger-rtt")
      if(EXISTS "${_rtt}/RTT/SEGGER_RTT.c")
        set(SEGGER_RTT_PATH "${_rtt}")
        break()
      endif()
      if(EXISTS "${_rtt}/SEGGER_RTT.c")
        set(SEGGER_RTT_PATH "${_rtt}")
        break()
      endif()
    endforeach()
  endif()
  if(NOT SEGGER_RTT_PATH)
    message(FATAL_ERROR
      "SEGGER RTT not found. Run ../../scripts/fetch_mcux_sdk.sh "
      "or pass -DSEGGER_RTT_PATH=/path/to/SEGGERMicro/RTT")
  endif()
  if(EXISTS "${SEGGER_RTT_PATH}/RTT/SEGGER_RTT.c")
    set(SEGGER_RTT_SRC "${SEGGER_RTT_PATH}/RTT/SEGGER_RTT.c")
    include_directories(${SEGGER_RTT_PATH}/RTT ${SEGGER_RTT_PATH}/Config)
  else()
    set(SEGGER_RTT_SRC "${SEGGER_RTT_PATH}/SEGGER_RTT.c")
    include_directories(${SEGGER_RTT_PATH})
  endif()
  list(APPEND LOG_SOURCES ${SEGGER_RTT_SRC})
  message(STATUS "FreeRTOS log backend: SEGGER RTT (${SEGGER_RTT_PATH})")
elseif(LOG_BACKEND_UPPER STREQUAL "USB")
  add_compile_definitions(CFG_TUSB_MCU=OPT_MCU_KINETIS_K)
  if(NOT TINYUSB_PATH)
    foreach(_tusb IN ITEMS
        "${MCU_SDK_PATH}/middleware/tinyusb"
        "${MCU_SDK_PATH}/../src/tinyusb"
        "${MCU_SDK_PATH}/../tinyusb"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../.deps/src/tinyusb")
      if(EXISTS "${_tusb}/src/tusb.c")
        set(TINYUSB_PATH "${_tusb}")
        break()
      endif()
    endforeach()
  endif()
  if(NOT TINYUSB_PATH)
    message(FATAL_ERROR
      "TinyUSB not found. Run ../../scripts/fetch_mcux_sdk.sh "
      "or pass -DTINYUSB_PATH=/path/to/tinyusb")
  endif()
  include_directories(${TINYUSB_PATH}/src)
  list(APPEND APP_SOURCES src/usb_descriptors.c)
  list(APPEND LOG_SOURCES
    ${TINYUSB_PATH}/src/tusb.c
    ${TINYUSB_PATH}/src/common/tusb_fifo.c
    ${TINYUSB_PATH}/src/device/usbd.c
    ${TINYUSB_PATH}/src/device/usbd_control.c
    ${TINYUSB_PATH}/src/class/cdc/cdc_device.c
    ${TINYUSB_PATH}/src/portable/nxp/khci/dcd_khci.c
  )
  message(STATUS "FreeRTOS log backend: USB CDC ACM (${TINYUSB_PATH})")
else()
  message(FATAL_ERROR "LOG_BACKEND must be USB or RTT (got '${LOG_BACKEND}')")
endif()

add_executable(${PROJECT_NAME}.elf
  ${APP_SOURCES}
  ${DEVICE_STARTUP}
  ${DEVICE_SYSTEM}
  ${SDK_DRIVERS}
  ${FREERTOS_SOURCES}
  ${LOG_SOURCES}
)

target_link_options(${PROJECT_NAME}.elf PRIVATE
  ${CPU_FLAGS}
  -T${MCU_LD}
  -Wl,--gc-sections
  -Wl,-Map=${PROJECT_NAME}.map
  --specs=nano.specs
  --specs=nosys.specs
)

target_link_libraries(${PROJECT_NAME}.elf PRIVATE m c nosys)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
  COMMENT "Generating HEX/BIN and printing size"
)
