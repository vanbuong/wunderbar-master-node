# nRF51822 BT master (Phase 0)

LED blink + SPI slave (`wb_bt_frame`) + GP1 ready IRQ + **SEGGER RTT** logging.
**No SoftDevice** in Phase 0 — flash this app alone for host-link bring-up.

## Requirements

- **nRF5 SDK 12.1.0** unpacked somewhere (same tree you used for blinky)
- `arm-none-eabi-gcc` on `PATH`
- Optional: `nrfjprog` for flash; **J-Link RTT Viewer** / Ozone / SES for logs

```bash
export NRF5_SDK_ROOT=/path/to/nRF5_SDK_12.1.0
cd apps/nrf51_bt_master
make
make flash   # if nrfjprog available
```

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

## Phase 0 behaviour

1. LED toggles ~1 Hz  
2. Asserts GP1 with an `IDLE` frame queued  
3. On each SPI transaction, parses host frame; replies `PONG` to `PING`, else `IDLE`  
4. RTT logs SPI events and a 5 s heartbeat  

## Later phases

SoftDevice **S130** + Central scan/connect — see `docs/nrf51822_master_plan.md`.
When adding S130, switch to the SDK SoftDevice linker script and flash S130 hex first.
