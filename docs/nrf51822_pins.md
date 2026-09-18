# nRF51822 pin map (WunderBar master module)

BLE SoC on the master PCB: **nRF51822 QFAA-G0 (revision 2)**. Talks to MK24 over SPI + 2 GPIOs; talks to sensor modules over BLE.

## MK24 ↔ nRF host interface

| Signal | MK24 | nRF51822 | Notes |
|--------|------|----------|-------|
| `BT_SPI_SCK` | **PTA15** | **P0.05** | SPI clock (MK24 master) |
| `BT_SPI_MOSI` | **PTA16** | **P0.01** | Host → nRF |
| `BT_SPI_MISO` | **PTA17** | **P0.00** | nRF → host |
| `BT_SPI_SSEL` | **PTA14** | **P0.03** | Chip select (active low) |
| `BT_MCU_GP1` | **PTA10** | **P0.02** | nRF → host **ready-to-send / IRQ** |
| `BT_MCU_GP2` | **PTA13** | **P0.04** | Host → nRF control / legacy bootloader enter |

nRF is **SPI slave**. Do not confuse with BLE Central (“BT master”) role.

WiFi uses a **different** SPI bank (`PTD10–14`); do not mux them together.

## nRF onboard

| Function | Pin |
|----------|-----|
| HF crystal 16 MHz | XC1 / XC2 |
| LF crystal 32.768 kHz | P0.26 / P0.27 |
| Status LED (`BT_LED1`) | **P0.29** |
| SWDIO / SWDCLK | pins 23 / 24 |

## See also

- [`nrf51822_master_plan.md`](nrf51822_master_plan.md) — firmware architecture and phases
