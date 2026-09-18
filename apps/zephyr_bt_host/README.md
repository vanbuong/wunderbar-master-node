# Zephyr SPI master stub toward nRF51822 (Phase 1 WBBT host protocol).

USB CDC or SEGGER RTT console. Speaks `wb_bt_frame` over SPI0 @ 1 MHz.

## Build

```bash
# from repo root (same as blinky)
west build -b wunderbar_master/mk64f12 apps/zephyr_bt_host \
  -d build-zephyr-bt -- -DBOARD_ROOT=$PWD

# or
./scripts/build.sh zephyr-bt
```

Flash both this image and `apps/nrf51_bt_master` Phase 1. Connect J-Link
for RTT on either MCU as needed. Watch host logs for `PING -> PONG` and
`GET_INFO caps=… id="wb-nrf51-p1"`.

## Pins

See `docs/nrf51822_pins.md`. Host uses SPI0 (PTA14–17) + `bt-ready` PTA10.
