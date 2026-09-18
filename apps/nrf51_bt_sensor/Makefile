# Build nRF51822 IMU sensor peripheral (S130 + MPU I2C + WBS GATT)
#   export NRF5_SDK_ROOT=/path/to/nRF5_SDK_12.1.0
#   make

PROJECT_NAME := wb_nrf51_bt_sensor
OUTPUT_DIR   := build

ifndef NRF5_SDK_ROOT
  $(error Set NRF5_SDK_ROOT to your nRF5 SDK 12.1.0 path)
endif

GNU_INSTALL_ROOT ?=
PREFIX           := $(GNU_INSTALL_ROOT)arm-none-eabi-
CC               := $(PREFIX)gcc
OBJCOPY          := $(PREFIX)objcopy
SIZE             := $(PREFIX)size

WB_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
SDK     := $(NRF5_SDK_ROOT)

SRC_FILES := \
  src/main.c \
  src/wb_twi.c \
  src/wb_mpu.c \
  src/wb_ahrs.c \
  src/wb_wbs_s.c \
  $(SDK)/components/libraries/util/app_error.c \
  $(SDK)/components/libraries/util/app_error_weak.c \
  $(SDK)/components/libraries/util/app_util_platform.c \
  $(SDK)/components/libraries/util/nrf_assert.c \
  $(SDK)/components/libraries/hardfault/hardfault_implementation.c \
  $(SDK)/components/libraries/timer/app_timer.c \
  $(SDK)/components/drivers_nrf/common/nrf_drv_common.c \
  $(SDK)/components/drivers_nrf/clock/nrf_drv_clock.c \
  $(SDK)/components/drivers_nrf/twi_master/nrf_drv_twi.c \
  $(SDK)/components/ble/common/ble_srv_common.c \
  $(SDK)/components/ble/common/ble_advdata.c \
  $(SDK)/components/softdevice/common/softdevice_handler/softdevice_handler.c \
  $(SDK)/external/segger_rtt/SEGGER_RTT.c \
  $(SDK)/external/segger_rtt/SEGGER_RTT_printf.c \
  $(SDK)/components/toolchain/system_nrf51.c \
  $(SDK)/components/toolchain/gcc/gcc_startup_nrf51.S

INC := \
  -Iboard \
  -Iconfig \
  -Isrc \
  -I$(WB_ROOT)/lib/bt/include \
  -I$(SDK)/components/device \
  -I$(SDK)/components/toolchain \
  -I$(SDK)/components/toolchain/cmsis/include \
  -I$(SDK)/components/toolchain/gcc \
  -I$(SDK)/components/drivers_nrf/clock \
  -I$(SDK)/components/drivers_nrf/common \
  -I$(SDK)/components/drivers_nrf/delay \
  -I$(SDK)/components/drivers_nrf/twi_master \
  -I$(SDK)/components/drivers_nrf/hal \
  -I$(SDK)/components/libraries/util \
  -I$(SDK)/components/libraries/hardfault \
  -I$(SDK)/components/libraries/timer \
  -I$(SDK)/components/libraries/scheduler \
  -I$(SDK)/components/libraries/log \
  -I$(SDK)/components/libraries/log/src \
  -I$(SDK)/components/libraries/experimental_section_vars \
  -I$(SDK)/components/libraries/fstorage \
  -I$(SDK)/components/ble/common \
  -I$(SDK)/components/softdevice/common/softdevice_handler \
  -I$(SDK)/external/segger_rtt \
  -I$(SDK)/components/softdevice/s130/headers \
  -I$(SDK)/components/softdevice/s130/headers/nrf51

LINKER_SCRIPT := board/wb_nrf51_s130_periph.ld
SD_HEX := $(SDK)/components/softdevice/s130/hex/s130_nrf51_2.0.1_softdevice.hex

CFLAGS := -mcpu=cortex-m0 -mthumb -mabi=aapcs \
  -Wall -Werror -O2 -g3 \
  -DNRF51 -DNRF51822 -DBOARD_PCA10028 \
  -DSOFTDEVICE_PRESENT -DS130 -DBLE_STACK_SUPPORT_REQD -DSWI_DISABLE0 \
  -DNRF_SD_BLE_API_VERSION=2 \
  -D__HEAP_SIZE=0 -D__STACK_SIZE=2048 \
  -DNRF_LOG_USES_RTT=1 \
  $(INC)

ASMFLAGS := -mcpu=cortex-m0 -mthumb -x assembler-with-cpp \
  -DNRF51 -DNRF51822 -DSOFTDEVICE_PRESENT -DS130 \
  -DBLE_STACK_SUPPORT_REQD -DSWI_DISABLE0 \
  -D__HEAP_SIZE=0 -D__STACK_SIZE=2048 \
  $(INC)

LDFLAGS := -mcpu=cortex-m0 -mthumb -mabi=aapcs \
  -L$(SDK)/components/toolchain/gcc \
  -T$(LINKER_SCRIPT) \
  -Wl,-Map=$(OUTPUT_DIR)/$(PROJECT_NAME).map \
  -Wl,--gc-sections \
  --specs=nano.specs

LDLIBS := -lc -lnosys -lm

OBJ := $(addprefix $(OUTPUT_DIR)/,$(notdir $(SRC_FILES:.c=.o)))
OBJ := $(OBJ:.S=.o)
OBJ := $(OBJ:.s=.o)

vpath %.c $(sort $(dir $(SRC_FILES)))
vpath %.S $(SDK)/components/toolchain/gcc
vpath %.s $(SDK)/components/toolchain/gcc

.PHONY: all clean flash flash_sd size

all: $(OUTPUT_DIR)/$(PROJECT_NAME).hex size

$(OUTPUT_DIR):
	mkdir -p $@

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/%.o: %.S | $(OUTPUT_DIR)
	$(CC) $(ASMFLAGS) -c $< -o $@

$(OUTPUT_DIR)/$(PROJECT_NAME).out: $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OUTPUT_DIR)/$(PROJECT_NAME).hex: $(OUTPUT_DIR)/$(PROJECT_NAME).out
	$(OBJCOPY) -O ihex $< $@

size: $(OUTPUT_DIR)/$(PROJECT_NAME).out
	$(SIZE) $<

flash_sd:
	nrfjprog -f nrf51 --program $(SD_HEX) --chiperase

flash: $(OUTPUT_DIR)/$(PROJECT_NAME).hex
	nrfjprog -f nrf51 --program $< --sectorerase -r

clean:
	rm -rf $(OUTPUT_DIR)
