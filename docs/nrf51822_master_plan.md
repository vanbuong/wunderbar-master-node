# nRF51822 BLE master plan (WunderBar)

Plan for the onboard **nRF51822 QFAA-G0 (rev 2)** that acts as **BLE Central** toward sensor
slaves, and as **SPI slave** toward the MK24 host.

## Locked decisions

| Topic | Choice |
|-------|--------|
| Sensors / GATT | **Greenfield** — redefine services; no legacy module compatibility required |
| SDK | **nRF5 SDK 12.1.0** + SoftDevice **S130** (Central/multi-link). Blinky already proven on this silicon |
| SPI ABI | **New versioned frames** (`WBBT` v1) — see [`nrf51822_spi_host.md`](nrf51822_spi_host.md); not legacy-compatible |
| Shared header | `lib/bt/include/wb_bt_frame.h` |

Why S130 on SDK 12.1: officially supports nRF51 Central (e.g. `ble_app_multilink_central`, up to 8 links). Newer SDKs drop/weaken nRF51. S110 is peripheral-only — wrong role.

## Roles (do not confuse “master”)

| Domain | Role | Meaning |
|--------|------|---------|
| **BLE** | Central | Scans/connects to sensor nRFs, collects data |
| **SPI** | Slave | MK24 is SPI master; nRF raises GP1 when TX ready |

```
[sensor nRF] --BLE--> [nRF51822 on master] --SPI+GPIO--> [MK24] --WiFi--> cloud / app
```

## Hardware

See [`nrf51822_pins.md`](nrf51822_pins.md). Summary:

| Net | MK24 | nRF | Role |
|-----|------|-----|------|
| SPI SCK/MOSI/MISO/SSEL | PTA15/16/17/14 | P0.05/01/00/03 | Host SPI |
| GP1 ready | PTA10 | P0.02 | nRF → host IRQ |
| GP2 ctrl | PTA13 | P0.04 | Host → nRF (reserved) |
| LED | — | P0.29 | Status |

## Software architecture

```
┌─────────────────────────────────────────┐
│  Host SPI slave + wb_bt_frame codec     │
├─────────────────────────────────────────┤
│  Sensor session manager (greenfield)    │
├─────────────────────────────────────────┤
│  SoftDevice S130 (BLE Central)          │
├─────────────────────────────────────────┤
│  Board: SPIS, GP1/GP2, LED, LFCLK       │
└─────────────────────────────────────────┘
```

## GATT (greenfield sketch)

Define later as 128-bit vendor UUIDs under a single primary service per sensor type, e.g.:

- Device Info (name, fw, battery)
- Sensor Data (notify characteristic, packed samples)
- Sensor Config (write)

Central discovers by service UUID filter, not legacy names/passkeys.

## Phased delivery

### Phase 0 — Bring-up (current scaffold)
- App: `apps/nrf51_bt_master/`
- Blink LED P0.29
- SPIS + fixed 64-byte `PONG`/`IDLE` frames
- Assert GP1 when TX ready
- Build against external `NRF5_SDK_ROOT` (12.1.0)

### Phase 1 — SPI host protocol
- Full `wb_bt_frame` TX queue, CRC check, `PING`/`PONG`/`CMD`
- MK24 Zephyr SPI master stub + DTS pins

### Phase 2 — BLE Central single slave
- S130 init, scan, connect to one test peripheral (nRF DK or custom sensor)
- Forward notify payload as `DATA` frames over SPI

### Phase 3 — Multi-sensor
- Up to N concurrent links (S130 budget vs 16 KB RAM)
- Bonding optional (LE Secure Connections if needed)

### Phase 4 — Polish
- LED codes, watchdog, DFU strategy

## Build / flash (SDK 12.1.0)

```bash
# Place or symlink SDK (contains components/, examples/, …)
export NRF5_SDK_ROOT=/path/to/nRF5_SDK_12.1.0
export GNU_INSTALL_ROOT=/usr/  # arm-none-eabi-gcc

cd apps/nrf51_bt_master
make          # builds Phase 0 app
# SoftDevice once:
# nrfjprog -f nrf51 --program $NRF5_SDK_ROOT/components/softdevice/s130/hex/s130_*.hex --chiperase
# make flash
```

SoftDevice hex must match the linker script (`s130_nrf51_2.0.1` with SDK 12.1).

## Risks

1. **16 KB RAM** — keep Central link count and buffers modest.  
2. SDK 12.1 SPIS + SoftDevice IRQ priorities must follow Nordic examples.  
3. Confirm flash/RAM variant of the onboard QFAA marking when linking.

## Open (non-blocking)

- Exact vendor UUID base and sensor ID table (Phase 2)  
- Whether GP2 resets nRF into bootloader or is unused in v1  
