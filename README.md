# WunderBar Master Node (MK24FN1M0VDC12)

First firmware projects for the Relayr **WunderBar** master module:

| MCU | Package | Flash / SRAM |
|-----|---------|--------------|
| MK24FN1M0VDC12 | 121 XFBGA | 1 MB / 256 KB |

## Hardware clocks (from schematic)

| Source | Frequency | Pins | Load caps |
|--------|-----------|------|-----------|
| Y1 system crystal | **12 MHz** | EXTAL0 / XTAL0 | C17, C18 = **12 pF** |
| X8 RTC crystal | **32.768 kHz** | EXTAL32 / XTAL32 | C104, C105 = **12 pF** |

### Target RUN clock (MCG PEE)

| Clock | Frequency |
|-------|-----------|
| Core / system | **120 MHz** |
| Bus | 60 MHz |
| FlexBus | 40 MHz |
| Flash | 24 MHz |

**PLL:** `12 MHz ÷ 3 × 30 = 120 MHz`  
(`PRDIV=2`, `VDIV=6` — same math as Zephyr Hexiwear K64)

On-chip OSC load capacitors are left at **0 pF** because the board already has external 12 pF parts.

## Correct SDK / SoC support

| Stack | What to use | Why |
|-------|-------------|-----|
| **MCUXpresso SDK** | Export **FRDM-K64F** SDK (ARM GCC + FreeRTOS) from [MCUXpresso SDK Builder](https://mcuxpresso.nxp.com/en/builder) | Official 2.x packages list **MK24FN1M0VDC12** with the K64 1 MB family (`devices/MK64F12`). Dedicated MK24 folders were removed from current `mcux-sdk` mainline. |
| **Zephyr** | Out-of-tree board `wunderbar_master` on SoC `mk64f12` | NXP treats **FRDM-K64F** as the Kinetis K enablement board for K24/K63/K64. There is no separate in-tree MK24 board. |

Do **not** copy FRDM-K64F’s default **50 MHz** EXTAL settings. This module uses a **12 MHz** crystal (like Hexiwear).

## Repository layout

```
boards/relayr/wunderbar_master/   Zephyr HWMv2 board (12 MHz + PTA29 LED)
apps/zephyr_blinky/               Zephyr CMake blinky (USB CDC + RTT images)
apps/mcux_freertos_blinky/        MCUXpresso SDK + FreeRTOS CMake blinky
lib/log/                          Portable wb_log module (level + backends)
tests/unity/                      Unity host unit tests for wb_log
tests/ztest/wb_log/               Zephyr ztest suite for wb_log
west.yml                          Zephyr west manifest (v4.4.2)
scripts/                          Host build helpers (firmware + tests)
.github/workflows/build.yml       CI: four images + Unity + ztest
```

LED: **PTA29** (`GPIOA` pin 29). Default polarity is active-high; flip it in the DTS / `LED_ACTIVE_HIGH` if your LED is wired active-low.

USB: dedicated **USB0_DP / USB0_DM** (no extra pinmux). The USB images enumerate as a **CDC ACM** serial port (`/dev/ttyACM*` on Linux, `COMx` on Windows). Baud rate is ignored.

A second firmware per OS prints the same blink log over **SEGGER RTT** (J-Link SWD; no USB cable required).

---

## Build both (Zephyr + FreeRTOS)

From this repository, after installing west, CMake, Ninja, and an ARM GCC (Zephyr SDK **or** `arm-none-eabi-gcc`):

```bash
./scripts/build.sh          # all four images (Zephyr/FreeRTOS × USB/RTT)
./scripts/build.sh zephyr   # Zephyr USB + RTT
./scripts/build.sh freertos # MCUX + FreeRTOS USB + RTT
./scripts/build.sh test     # Unity (host) + Zephyr ztest
```

Outputs:

| Image | Path |
|-------|------|
| Zephyr USB CDC | `build-zephyr/zephyr/zephyr.elf` |
| Zephyr RTT | `build-zephyr-rtt/zephyr/zephyr.elf` |
| FreeRTOS USB CDC | `build-freertos/wunderbar_freertos_blinky.elf` |
| FreeRTOS RTT | `build-freertos-rtt/wunderbar_freertos_blinky.elf` |

GitHub Actions builds the four images and runs **Unity** + **ztest** on every push/PR.

### Log module (`lib/log`)

Both firmwares use **`wb_log`** (`WB_LOGI` / `WB_LOGE` / …). Messages look like `[I] LED ON` and go through a pluggable backend:

| Backend | Use |
|---------|-----|
| `wb_log_stdio_backend()` | Firmware — `fwrite(stdout)` → FreeRTOS `_write` (USB/RTT) or Zephyr console |
| `wb_log_stub_backend()` | Unit tests — capture buffer + `wb_log_stub_contains()` |

```bash
./scripts/build.sh test
# or separately:
./scripts/fetch_unity.sh
cmake -S tests/unity -B build-unity -G Ninja && cmake --build build-unity && ctest --test-dir build-unity
west build -b unit_testing tests/ztest/wb_log -t run
```

---

## 1) Zephyr blinky (recommended first bring-up)

Zephyr includes its own kernel/scheduler (this is the usual Zephyr path; you do not also link FreeRTOS into the same image).

### Toolchain

1. Install [West](https://docs.zephyrproject.org/latest/develop/west/install.html): `pip install west`
2. Install the [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/toolchains/zephyr_sdk.html) (or another ARM toolchain Zephyr accepts). Zephyr 4.4 needs SDK **1.0.x**.

### Workspace

`west init -l` puts the workspace in the **parent** of this repo, and the repo folder must be named `wunderbar-master-node` (see `self.path` in `west.yml`):

```bash
mkdir wb-zephyr-workspace
cd wb-zephyr-workspace
west init -l ../wunderbar-master-node
west update
```

`./scripts/build.sh zephyr` creates that sibling workspace if needed (including when this checkout is named something else, e.g. `workspace`).

### Build & flash

```bash
west build -b wunderbar_master/mk64f12 \
  wunderbar-master-node/apps/zephyr_blinky \
  -p always

west flash
```

J-Link device string: `MK24FN1M0xxx12`.

Plug the module USB into the host and open the CDC port to see `LED ON` / `LED OFF` (wait up to ~5 s after reset if you want the banner; the LED still blinks if nothing is attached):

```bash
picocom -b 115200 /dev/ttyACM0
```

RTT image (J-Link connected over SWD):

```bash
west build -b wunderbar_master/mk64f12 \
  wunderbar-master-node/apps/zephyr_blinky \
  -d build-zephyr-rtt -- \
  -DEXTRA_CONF_FILE=rtt.conf \
  -DEXTRA_DTC_OVERLAY_FILE=rtt.overlay

JLinkRTTClient
```

---

## 2) MCUXpresso SDK + FreeRTOS blinky

Separate bare-metal/SDK image that runs **FreeRTOS** and toggles PTA29.

### Get the SDK

**Option A — GitHub (no NXP login)**

```bash
./scripts/fetch_mcux_sdk.sh
# then: MCU_SDK_PATH=$PWD/.deps/mcux-sdk
```

**Option B — MCUXpresso SDK Builder zip**

1. Open [MCUXpresso SDK Builder](https://mcuxpresso.nxp.com/en/builder)
2. Board: **FRDM-K64F**
3. Toolchain: **ARM GCC**
4. Include **FreeRTOS**
5. Download and unzip (example path: `C:/nxp/SDK_2.x_FRDM-K64F`)

This app replaces the board clock files with WunderBar’s **12 MHz / 32.768 kHz** configuration in `apps/mcux_freertos_blinky/board/clock_config.*`.

Early boot matches Zephyr: **do not write `RTC->CR`**, release **PMC ACKISO**, and disable **SYSMPU** (required for USB). `./scripts/fetch_mcux_sdk.sh` also clones **TinyUSB 0.17.0** for the CDC console.

### Build

```bash
./scripts/build.sh freertos
```

Or CMake directly:

```bash
cd apps/mcux_freertos_blinky
cmake -S . -B build -G Ninja \
  -DMCU_SDK_PATH=/path/to/SDK_2.x_FRDM-K64F \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Output: `build/wunderbar_freertos_blinky.elf` (or `build-freertos/` / `build-freertos-rtt/` when using `scripts/build.sh`) plus `.hex` / `.bin`.

USB CDC logs (`LED toggle (FreeRTOS USB)`) appear on the host serial port after you open a terminal (DTR). RTT logs (`LED toggle (FreeRTOS RTT)`) appear in J-Link RTT Viewer / `JLinkRTTClient`. The LED is turned on in `main` before the scheduler starts, then toggles every 500 ms.

---

## Quick clock cheat-sheet

| Setting | Value |
|---------|-------|
| `BOARD_XTAL0_CLK_HZ` / `OSC_XTAL0_FREQ` | `12000000` |
| `BOARD_XTAL32K_CLK_HZ` | `32768` |
| OSC mode | Low-power crystal (`kOSC_ModeOscLowPower` / `CONFIG_OSC_LOW_POWER`) |
| `MCG_PRDIV0` | `0x02` (÷3) |
| `MCG_VDIV0` | `0x06` (×30) |
| Core clock | `120000000` |
| RTC ERCLK32K source | RTC oscillator |

Optional: regenerate `clock_config.c/h` later with **MCUXpresso Config Tools → Clocks** for processor `MK24FN1M0xxx12` / package `MK24FN1M0VDC12`, then drop the files into `apps/mcux_freertos_blinky/board/`.
