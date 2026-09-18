# nRF51822 BT master (Phase 2)

SPI host protocol (Phase 1) **plus SoftDevice S130 BLE Central** (1 link):
scan / connect / greenfield WBS GATT notify → `DATA`/`EVT` over SPI.

## Requirements

- **nRF5 SDK 12.1.0** + SoftDevice **S130 2.0.1**
- **GNU Arm Embedded 10.3-2021.10** (GCC 10.3.1)
- Optional: `nrfjprog`, J-Link RTT Viewer

```bash
# from repo root
./scripts/fetch_arm_gcc_10_3_1.sh
./scripts/fetch_nrf5_sdk.sh
export PATH="$PWD/.deps/gcc-arm-none-eabi-10.3-2021.10/bin:$PATH"
export NRF5_SDK_ROOT="$PWD/.deps/nRF5_SDK_12.1.0"
cd apps/nrf51_bt_master
make
make flash_sd   # SoftDevice once (chip erase)
make flash      # app
```

## Host CMDs (SPI)

| CMD | Action |
|-----|--------|
| `GET_INFO` | caps include `WB_BT_CAP_BLE` |
| `SCAN_START` | filter WBS UUID; `payload[0]&1` = auto-connect first hit |
| `SCAN_STOP` | stop scan |
| `CONNECT` | `addr_type` + 6-byte addr |
| `DISCONNECT` | drop link |

Notifies arrive as `DATA`; link/scan as `EVT` — see `docs/nrf51822_spi_host.md`
and `lib/bt/include/wb_bt_gatt.h`.

## Test peripheral

Use `apps/nrf51_bt_sensor` (MPU I2C + roll/pitch/yaw + motion notify) on a
second nRF51822, or any peripheral that advertises the WBS service UUID
(`57420001-4253-1000-8000-00805f9b34fb`) with notify characteristic `0x0002`.

## Pins / RTT

Unchanged from Phase 1 — see `docs/nrf51822_pins.md`.
