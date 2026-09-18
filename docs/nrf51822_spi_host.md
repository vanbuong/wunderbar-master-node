# nRF51822 ↔ MK24 SPI host ABI (v1)

Clean, versioned framing for the host link. **Not** byte-compatible with legacy
relayr frames — sensors are being reimplemented, so we optimize for clarity.

## Physical layer

| Item | Value |
|------|-------|
| Mode | SPI mode **0** (CPOL=0, CPHA=0), MSB first |
| Role | MK24 = master, nRF = slave (`SPIS`) |
| Clock | Start at **1 MHz**; raise later if stable |
| CSN | `BT_SPI_SSEL` active low |
| Ready | `BT_MCU_GP1`: nRF **asserts high** when ≥1 outbound frame queued; host may clock SPI; nRF clears when TX queue empty after the transaction |

Idle MOSI/MISO: slave clocks out `0xFF` when it has nothing to say.

## Frame layout (little-endian)

Every SPI transaction exchanges **exactly one frame each direction** (full duplex).
If a side has no message, it sends an **IDLE** frame.

```
Offset  Size  Field
0       4     magic = 'W','B','B','T'  (0x54 0x42 0x42 0x57 LE as u32 0x57424254)
4       1     version = 1
5       1     flags
6       1     seq          (0–255, increment per direction)
7       1     type         (see below)
8       1     sensor_id    (0xFF = none / broadcast / host)
9       1     field_id
10      2     payload_len  (0 … PAYLOAD_MAX)
12      N     payload
12+N    2     reserved (pad)
14+N    2     crc16-ccitt over bytes [0 .. 14+N-1) excluding crc
              (poly 0x1021, init 0xFFFF)
```

`PAYLOAD_MAX` = **48** → header+payload+pad+crc = **64** bytes fixed
(`WB_BT_FRAME_SIZE`). Unused payload bytes are `0x00`.

Shared C header: `lib/bt/include/wb_bt_frame.h`.

## `type` values

| Code | Name | Direction | Meaning |
|------|------|-----------|---------|
| 0x00 | `IDLE` | either | No payload; keep link alive / fill duplex |
| 0x01 | `PING` | host→nRF | Expect `PONG` |
| 0x02 | `PONG` | nRF→host | Reply to `PING` |
| 0x10 | `CMD` | host→nRF | Host command (`field_id` = opcode) |
| 0x11 | `RSP` | nRF→host | Command result |
| 0x20 | `EVT` | nRF→host | Async sensor / link event |
| 0x21 | `DATA` | nRF→host | Sensor sample payload |

## Host `CMD` opcodes (`field_id` when `type=CMD`)

| Opcode | Name | Payload |
|--------|------|---------|
| 0x01 | `GET_INFO` | empty → RSP: caps + fw id string |
| 0x02 | `SCAN_START` | optional `payload[0]` flags (`WB_BT_SCAN_FLAG_AUTO_CONNECT`) |
| 0x03 | `SCAN_STOP` | empty |
| 0x04 | `CONNECT` | `addr_type(1)` + `addr(6)` |
| 0x05 | `DISCONNECT` | empty (Phase 2 single link) |
| 0x06 | `SET_SENSOR_CFG` | sensor-specific (Phase 3+) |

RSP for these CMDs: `payload[0] = WB_BT_ERR_*`.

## `EVT.field_id` (nRF → host)

| Code | Name | Payload |
|------|------|---------|
| 0x01 | `SCAN_REPORT` | `addr_type(1)` + `addr(6)` + `rssi(1 signed)` |
| 0x02 | `CONNECTED` | `addr_type(1)` + `addr(6)` |
| 0x03 | `DISCONNECTED` | `reason(1)` |
| 0x04 | `READY` | empty (GATT discovered, notify enabled) |

`DATA` frames carry notify bytes (`sensor_id=0`, `field_id=0x0002` data UUID).

## Sensor IDs (greenfield)

Phase 2 uses `sensor_id=0` for the single link. `0xFF` = N/A / scan.

## Ready / flow

1. nRF enqueues `EVT`/`DATA`/`RSP`/`PONG` → asserts GP1.  
2. Host sees GP1, pulls CSN, clocks 64 bytes (may send `CMD` or `IDLE` on MOSI).  
3. nRF returns one queued frame on MISO (or `IDLE`).  
4. If queue still non-empty, GP1 stays asserted; else deasserted.  
5. Host may also poll periodically with `PING` even if GP1 is low.

**Full duplex latency:** the frame on MISO is whatever was already queued *before*
this clock. A `PING`/`CMD` on MOSI is handled after the transfer completes; the
reply (`PONG`/`RSP`) is returned on a **subsequent** transaction (often right
after GP1 asserts). Host stubs should drain with `IDLE` until the expected type
appears.

Corrupt CRC → nRF may reply `RSP` with `payload[0]=WB_BT_ERR_BAD_CRC` (Phase 1).

### `GET_INFO` RSP payload

| Offset | Size | Field |
|--------|------|-------|
| 0 | 1 | caps (`WB_BT_CAP_SPI`, `WB_BT_CAP_BLE`, …) |
| 1… | ≤47 | NUL-terminated firmware id string |

## Versioning

- `version` must be **1** for this ABI.  
- Unknown `type`: ignore frame, do not fault the link.  
- Bump `version` only for breaking layout changes.
