# nRF51822 BLE master plan (WunderBar)

Plan for the onboard **nRF51822 QFAA-G0 (rev 2)** that acts as **BLE Central** toward sensor
slaves, and as **SPI slave** toward the MK24 host.

No nRF firmware exists in this repo yet. This document is the implementation blueprint.

## Roles (do not confuse “master”)

| Domain | Role | Meaning |
|--------|------|---------|
| **BLE** | Central / “BT master” | Scans, connects to WunderBar sensor modules (nRF peripherals), collects data |
| **SPI** | Slave | MK24 is SPI master; nRF answers host transactions and raises an IRQ when TX is ready |

```
[sensor nRF] --BLE--> [nRF51822 on master] --SPI+GPIO--> [MK24] --WiFi--> cloud / app
     (slave)              (BLE central +               (host)
                          SPI slave)
```

## Hardware (from schematic + legacy firmware)

### MK24 ↔ nRF SPI / GPIO

| Net | MK24 | nRF51822 | Direction (host view) |
|-----|------|----------|------------------------|
| `BT_SPI_SCK` | **PTA15** | **P0.05** | MK24 → nRF |
| `BT_SPI_MOSI` | **PTA16** | **P0.01** | MK24 → nRF |
| `BT_SPI_MISO` | **PTA17** | **P0.00** | nRF → MK24 |
| `BT_SPI_SSEL` | **PTA14** | **P0.03** | MK24 → nRF (CSN, active low) |
| `BT_MCU_GP1` | **PTA10** | **P0.02** | nRF → MK24 — **SPI ready-to-send / IRQ** |
| `BT_MCU_GP2` | **PTA13** | **P0.04** | Host → nRF — legacy **bootloader enter / control** |

Matches relayr / MikroE `spi_slave_config.c`:

- `SPIS_MISO=0`, `SPIS_MOSI=1`, `SPIS_CSN=3`, `SPIS_SCK=5`, `SPIS_RDY_TO_SEND=2`

### nRF onboard

| Function | Pin |
|----------|-----|
| HFXTAL 16 MHz | XC1 / XC2 |
| LFXTAL 32.768 kHz | P0.26 / P0.27 |
| Status LED `BT_LED1` | **P0.29** (active high via 220 Ω) |
| SWD | SWDIO (23), SWDCLK (24) |

**Part:** QFAA-G0 rev 2 → plan for **256 KB flash / 16 KB RAM** unless marking proves 32 KB RAM.
SoftDevice choice and app RAM must fit that map.

## SoftDevice / SDK choice

Legacy WunderBar master used **Nordic SDK 6.1 + SoftDevice S120 v1.0.1** (multi-link Central).

| Option | Pros | Cons |
|--------|------|------|
| **A. Revive legacy S120 stack** | Known WunderBar sensor pairing / passkeys / SPI frame layout | Old SDK, hard to maintain, GCC/ninja only |
| **B. nRF5 SDK 12.x + S130** | Still nRF51-capable, better docs, multi-role | Port SPI + GATT client from legacy |
| **C. Zephyr on nRF51** | Aligns with MK24 Zephyr work | nRF51 BLE Central support is thin / painful; not recommended as v1 |

**Recommendation for v1:** **Option A or a thin reimplementation of Option A’s behaviour** on the newest SDK that still supports this silicon (prefer S130 if flash allows), keeping the **same SPI frame contract** so MK24 host code is stable.

Do **not** use S110 (peripheral-only) — this chip must be Central.

## Software architecture (nRF app)

```
┌─────────────────────────────────────────┐
│  Host SPI slave + frame codec           │  ← MK24 protocol
│  (ready IRQ on GP1, CSN transaction)    │
├─────────────────────────────────────────┤
│  Sensor session manager                 │  ← up to N links
│  scan / connect / bond / notify parse   │
├─────────────────────────────────────────┤
│  SoftDevice (S120/S130) BLE Central     │
├─────────────────────────────────────────┤
│  Board: SPIS, GP1/GP2, LED, LFCLK       │
└─────────────────────────────────────────┘
```

### Host SPI framing (conceptual, from legacy)

Legacy used fixed `spi_frame_t` records with:

- `data_id` — which sensor / client
- `field_id` — characteristic / field
- `operation` — read / write / event
- payload bytes

nRF queues outbound frames; asserts **GP1 (ready)** when a frame is waiting; MK24 clocks SPI and clears ready after consume. Inbound host commands (config, passkeys, connect requests) arrive on MOSI during transactions.

Exact frame layout should be copied from `wunderbar_common.h` / `spi_frame_t` in legacy firmware (or redefined once and versioned). **Freeze the ABI before writing MK24 driver.**

### BLE Central behaviour

1. Boot → LFCLK + SoftDevice + SPIS + LED heartbeat  
2. Load bonded peers / passkeys from flash (or wait for MK24 onboard config)  
3. Scan for known sensor names / service UUIDs  
4. Connect + optional MITM passkey (legacy used 6-digit passkeys per sensor)  
5. Enable notifications; push samples into SPI TX queue → assert GP1  
6. Handle disconnect / reconnect without hanging SPI

Target sensor set (classic WunderBar): HTU (temp/RH), gyro/accel, light, sound, bridge, IR — treat as pluggable `client_handling` modules.

## MK24 host side (companion, later)

| Piece | Notes |
|-------|--------|
| SPI master on **SPI0/1** using **PTA14–17** | Separate from WiFi SPI (PTD10–14) |
| GPIO input **PTA10** (GP1 IRQ) | Edge-triggered; drain SPI when ready |
| GPIO out **PTA13** (GP2) | Bootloader / reset-assist only if needed |
| Zephyr DTS | `spi` + `gpio` aliases `bt_spi`, `bt_rdy`, `bt_ctrl` |
| Host library | `lib/bt/wb_bt_host/` mirroring SPI frames |

Keep WiFi and BT host stacks independent; share only higher-level “sensor sample” types if useful.

## Proposed repo layout

```
apps/nrf51_bt_master/          # nRF app (separate toolchain / west or nRF5 SDK build)
boards/relayr/wunderbar_nrf/   # if Zephyr later; else board.h in SDK project
docs/nrf51822_pins.md          # pin table (this plan’s HW section)
docs/nrf51822_spi_host.md      # frozen SPI frame ABI
lib/bt/                        # optional shared frame defs (host + nRF)
```

Build: start with **nRF5 SDK + GCC** (or revive `slashdevteam/wunderbar-nrf-master` build) flashed via **BT SWD**, SoftDevice first then app.

## Phased delivery

### Phase 0 — Bring-up (no BLE)
- Blink `P0.29`
- SPIS loopback / fixed TX pattern; GP1 toggles when buffer ready
- MK24 (or logic analyzer) verifies PTA14–17 + PTA10

### Phase 1 — SPI host protocol
- Implement frame codec + TX queue + ready IRQ
- Host unit tests / Zephyr stub that reads frames
- Document ABI in `docs/nrf51822_spi_host.md`

### Phase 2 — BLE Central single slave
- SoftDevice init, scan, connect to one known peripheral
- Subscribe to one notify char; forward payload over SPI

### Phase 3 — Multi-sensor + bonding
- Passkey / bond store; reconnect; multiple concurrent links (S120/S130 limit)
- Onboarding mode driven by MK24 (legacy `onboard_*`)

### Phase 4 — Product polish
- LED status codes, watchdog, DFU strategy (GP2 / dual-bank if flash allows)
- Power: radio + SPIS idle current; MK24 may hold nRF in reset if a reset net exists (confirm on full schematic)

## Risks / constraints

1. **RAM**: SoftDevice + multi-link + SPI buffers on 16 KB is tight — budget early.  
2. **SPI vs radio timing**: keep SPIS IRQ short; queue BLE events to main context.  
3. **Legacy ABI vs clean redesign**: compatibility with old sensor modules may require legacy GATT/passkeys.  
4. **Rev 2 silicon**: stick to SoftDevices validated for EngRev/G0.  
5. **No shared reset line** in the crop — confirm how MK24 hard-resets nRF (power rail only?).

## Open decisions (need product choice)

1. Compatible with **stock WunderBar sensor modules**, or greenfield BLE GATT?  
2. SoftDevice: revive **S120** vs port to **S130**?  
3. SPI ABI: **byte-compatible with legacy** vs new versioned frames?  
4. DFU: SWD-only for now, or serial/BLE DFU later?

## Suggested first implementation PR

1. `docs/nrf51822_pins.md` (this HW map)  
2. Minimal nRF bring-up app (LED + SPIS + GP1) under `apps/nrf51_bt_master/`  
3. MK24 DTS stubs for BT SPI / IRQ (no full host driver yet)

After Phase 0 works on hardware, freeze SPI ABI and proceed to Phase 1–2.
