# Build Phase 0 nRF51822 app against nRF5 SDK 12.1.0
#   export NRF5_SDK_ROOT=/path/to/nRF5_SDK_12.1.0
#   make

PROJECT_NAME := wb_nrf51_bt_master
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

# SDK 12.1 paths (no SoftDevice for Phase 0)
SDK := $(NRF5_SDK_ROOT)

SRC_FILES := \
  src/main.c \
  src/wb_txq.c \
  $(SDK)/components/libraries/util/app_error.c \
  $(SDK)/components/libraries/util/app_error_weak.c \
  $(SDK)/components/libraries/util/app_util_platform.c \
  $(SDK)/components/libraries/util/nrf_assert.c \
  $(SDK)/components/libraries/hardfault/hardfault_implementation.c \
  $(SDK)/components/drivers_nrf/common/nrf_drv_common.c \
  $(SDK)/components/drivers_nrf/clock/nrf_drv_clock.c \
  $(SDK)/components/drivers_nrf/spi_slave/nrf_drv_spis.c \
  $(SDK)/components/drivers_nrf/nrf_soc_nosd/nrf_nvic.c \
  $(SDK)/components/drivers_nrf/nrf_soc_nosd/nrf_soc.c \
  $(SDK)/external/segger_rtt/SEGGER_RTT.c \
  $(SDK)/external/segger_rtt/SEGGER_RTT_printf.c \
  $(SDK)/components/toolchain/system_nrf51.c \
  $(SDK)/components/toolchain/gcc/gcc_startup_nrf51.S

INC := \
  -Iboard \
  -Iconfig \
  -I$(WB_ROOT)/lib/bt/include \
  -I$(SDK)/components/device \
  -I$(SDK)/components/toolchain \
  -I$(SDK)/components/toolchain/cmsis/include \
  -I$(SDK)/components/toolchain/gcc \
  -I$(SDK)/components/drivers_nrf/clock \
  -I$(SDK)/components/drivers_nrf/common \
  -I$(SDK)/components/drivers_nrf/delay \
  -I$(SDK)/components/drivers_nrf/spi_slave \
  -I$(SDK)/components/drivers_nrf/hal \
  -I$(SDK)/components/drivers_nrf/nrf_soc_nosd \
  -I$(SDK)/components/libraries/util \
  -I$(SDK)/components/libraries/hardfault \
  -I$(SDK)/components/libraries/log \
  -I$(SDK)/components/libraries/log/src \
  -I$(SDK)/components/boards \
  -I$(SDK)/external/segger_rtt \
  -I$(SDK)/components/softdevice/s130/headers \
  -I$(SDK)/components/softdevice/s130/headers/nrf51

# Use blank (no SoftDevice) xxaa linker from SDK peripheral examples
LINKER_SCRIPT := $(SDK)/components/toolchain/gcc/nrf51_xxaa.ld

CFLAGS := -mcpu=cortex-m0 -mthumb -mabi=aapcs \
  -Wall -Werror -O2 -g3 \
  -DNRF51 -DNRF51822 -DBOARD_PCA10028 -DNRF_SD_BLE_API_VERSION=2 \
  -D__HEAP_SIZE=0 -D__STACK_SIZE=2048 \
  -DNRF_SPIS_ENABLED=1 -DNRF_LOG_USES_RTT=1 \
  $(INC)

ASMFLAGS := -mcpu=cortex-m0 -mthumb -x assembler-with-cpp \
  -DNRF51 -DNRF51822 $(INC)

LDFLAGS := -mcpu=cortex-m0 -mthumb -mabi=aapcs \
  -L$(SDK)/components/toolchain/gcc \
  -T$(LINKER_SCRIPT) \
  -Wl,-Map=$(OUTPUT_DIR)/$(PROJECT_NAME).map \
  -Wl,--gc-sections \
  --specs=nano.specs -lc -lnosys -lm

OBJ := $(addprefix $(OUTPUT_DIR)/,$(notdir $(SRC_FILES:.c=.o)))
OBJ := $(OBJ:.S=.o)
OBJ := $(OBJ:.s=.o)

vpath %.c $(sort $(dir $(SRC_FILES)))
vpath %.S $(SDK)/components/toolchain/gcc
vpath %.s $(SDK)/components/toolchain/gcc

.PHONY: all clean flash size

all: $(OUTPUT_DIR)/$(PROJECT_NAME).hex size

$(OUTPUT_DIR):
	mkdir -p $@

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/%.o: %.S | $(OUTPUT_DIR)
	$(CC) $(ASMFLAGS) -c $< -o $@

$(OUTPUT_DIR)/$(PROJECT_NAME).out: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(OUTPUT_DIR)/$(PROJECT_NAME).hex: $(OUTPUT_DIR)/$(PROJECT_NAME).out
	$(OBJCOPY) -O ihex $< $@

size: $(OUTPUT_DIR)/$(PROJECT_NAME).out
	$(SIZE) $<

flash: $(OUTPUT_DIR)/$(PROJECT_NAME).hex
	nrfjprog -f nrf51 --program $< --sectorerase -r

clean:
	rm -rf $(OUTPUT_DIR)
