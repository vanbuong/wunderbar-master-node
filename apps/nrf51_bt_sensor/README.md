# nRF51822 IMU sensor (WBS peripheral)

SoftDevice **S130** BLE peripheral that reads an Invensense MPU-6xxx over I2C,
estimates **roll / pitch / yaw** (complementary filter; yaw drifts without a
magnetometer), classifies motion (**still / moving / tilt / face_down /
upside_down**), and notifies the WunderBar master over the greenfield **WBS**
GATT service.

## Schematic pins

| Function | nRF pin | Notes |
|----------|---------|--------|
| IMU SCL | P0.20 | 10k pull-up to 3V3 |
| IMU SDA | P0.21 | 10k pull-up to 3V3 |
| IMU INT | P0.22 | 10k pull-up (unused in v1 poll mode) |
| LED | P0.29 | Active-low (to 3V3 via 220Ω) |
| BTN2 | P0.28 | Active-low to GND; internal pull-up |
| LFXTAL | P0.26 / P0.27 | 32.768 kHz |

IMU **AD0** is tied high → I2C address **0x69** (driver also probes **0x68**).
**NCS** high selects I2C mode.

## BLE / payload

- Device name: `WB-IMU`
- Service UUID: `57420001-4253-1000-8000-00805f9b34fb` (see `lib/bt/include/wb_bt_gatt.h`)
- Data char `0x0002`: notify + read — packed `wb_bt_imu_sample_t` (14 bytes)
- Config char `0x0003`: write (reserved)

Angles are **centi-degrees**; accel is **milli-g**. See `lib/bt/include/wb_bt_imu.h`.

## Build / flash

```bash
# from repo root
./scripts/fetch_arm_gcc_10_3_1.sh
./scripts/fetch_nrf5_sdk.sh
export PATH="$PWD/.deps/gcc-arm-none-eabi-10.3-2021.10/bin:$PATH"
export NRF5_SDK_ROOT="$PWD/.deps/nRF5_SDK_12.1.0"
cd apps/nrf51_bt_sensor
make
make flash_sd   # SoftDevice once (chip erase)
make flash      # app
```

Requirements: **nRF5 SDK 12.1.0** + SoftDevice **S130 2.0.1**, **GCC 10.3.1**.

## Pairing with the master

1. Flash this image on the IMU nRF51822 board.
2. Flash `apps/nrf51_bt_master` on the WunderBar BT module.
3. From the SPI host, send `SCAN_START` with auto-connect (`payload[0] |= 1`).
4. Master filters advertisers that include the WBS UUID, connects, enables
   notify, and forwards samples as SPI `DATA` frames.

RTT (J-Link) prints motion changes and ~1 Hz attitude/accel status.
