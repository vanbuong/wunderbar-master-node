# nRF51822 BT master (Phase 0)

LED blink + SPI slave (`wb_bt_frame`) + GP1 ready IRQ.
**No SoftDevice** in Phase 0 — flash this app alone for host-link bring-up.

## Requirements

- **nRF5 SDK 12.1.0** unpacked somewhere (same tree you used for blinky)
- `arm-none-eabi-gcc` on `PATH`
- Optional: `nrfjprog` for flash

```bash
export NRF5_SDK_ROOT=/path/to/nRF5_SDK_12.1.0
cd apps/nrf51_bt_master
make
make flash   # if nrfjprog available
```

## Pins

See `docs/nrf51822_pins.md`. SPIS on P0.00/01/03/05, ready P0.02, LED P0.29.

## Phase 0 behaviour

1. LED toggles ~1 Hz  
2. Asserts GP1 with an `IDLE` frame queued  
3. On each SPI transaction, parses host frame; replies `PONG` to `PING`, else `IDLE`

## Later phases

SoftDevice **S130** + Central scan/connect — see `docs/nrf51822_master_plan.md`.
When adding S130, switch to the SDK SoftDevice linker script and flash S130 hex first.
