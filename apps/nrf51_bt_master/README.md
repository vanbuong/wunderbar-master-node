# nRF51822 BT master (Phase 1)

SPI slave with **TX queue**, CRC checks, `PING`/`PONG`, and `CMD`/`RSP`
(`GET_INFO`). LED + GP1 ready + **SEGGER RTT**. **No SoftDevice** yet —
flash this app alone for host-link bring-up.

## Requirements

- **nRF5 SDK 12.1.0** (CI / local fetch scripts unpack it)
- **GNU Arm Embedded Toolchain 10.3-2021.10** (GCC **10.3.1**) — same as CI
- Optional: `nrfjprog` for flash; **J-Link RTT Viewer** / Ozone / SES for logs

```bash
# from repo root
./scripts/fetch_arm_gcc_10_3_1.sh
./scripts/fetch_nrf5_sdk.sh
export PATH="$PWD/.deps/gcc-arm-none-eabi-10.3-2021.10/bin:$PATH"
export NRF5_SDK_ROOT="$PWD/.deps/nRF5_SDK_12.1.0"
cd apps/nrf51_bt_master
make
make flash   # if nrfjprog available
```

Pair with MK24 host stub: `apps/zephyr_bt_host/`.

## RTT logging

Logs go out the **BT SWD** port (SWDIO/SWDCLK on the nRF), not UART.

1. Connect J-Link to the nRF SWD header  
2. Flash the app  
3. Open **J-Link RTT Viewer** (or SES Debug Terminal / Ozone RTT)  
   - Target device: `NRF51822_XXAA`  
   - Interface: SWD  
4. You should see boot lines and periodic `alive xfer=…`

Macros: `WB_RTT_PRINTF` / `WB_RTT_WRITE` in `board/wb_rtt.h` (wraps `SEGGER_RTT_*`).
Up-buffer mode is **non-blocking skip** so a disconnected viewer never stalls SPIS.

## Pins

See `docs/nrf51822_pins.md`. SPIS on P0.00/01/03/05, ready P0.02, LED P0.29.

## Phase 1 behaviour

1. LED toggles ~1 Hz  
2. Asserts GP1 when the TX queue is non-empty  
3. On each SPI transaction: CRC-check host frame; `PING`→enqueue `PONG`;
   `CMD GET_INFO`→enqueue `RSP`; unknown/`CMD` BLE ops → error RSP  
4. MISO returns the previously queued frame (or synthetic `IDLE`)  
5. RTT logs SPI events and a 5 s heartbeat  

## Later phases

SoftDevice **S130** + Central scan/connect — see `docs/nrf51822_master_plan.md`.
When adding S130, switch to the SDK SoftDevice linker script and flash S130 hex first.
