# nRF51822 IMU sensor pins (schematic)

Companion to [`nrf51822_pins.md`](nrf51822_pins.md) for the **sensor** nRF51822
board that carries an Invensense MPU-6xxx over I2C.

| Signal | nRF51822 | Notes |
|--------|----------|--------|
| GYRO_SCL | P0.20 | I2C clock, 10k to VD33 |
| GYRO_SDA | P0.21 | I2C data, 10k to VD33 |
| GYRO_INT | P0.22 | IMU INT, 10k to VD33 |
| BTN2 | P0.28 | To GND; use internal pull-up |
| SENS2_LED | P0.29 | Active-low (R18 to VD33) |
| SENS2_XL1/2 | P0.26 / P0.27 | 32.768 kHz crystal |
| SWDIO / SWDCLK | pin 23 / 24 | Debug |

IMU: **AD0 → VD33** → I2C address **0x69**; **NCS → VD33** (I2C mode); **FSYNC → GND**.

Firmware: `apps/nrf51_bt_sensor/`. Payload: `lib/bt/include/wb_bt_imu.h`.
