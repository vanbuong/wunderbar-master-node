# GS1500M as a Zephyr WiFi offload driver (esp_at pattern)

## Short answer

Yes — the same pattern as in-tree `espressif,esp-at` applies. There is **no** GainSpan/GS1500M driver in Zephyr today; we would add an out-of-tree (or upstream) **UART AT WiFi offload** driver.

Today the WunderBar Zephyr app still uses the portable `lib/wifi/gs1500m` AT library + a custom state machine. That is fine for bring-up; moving to `wifi_mgmt` + socket offload is the Zephyr-native end state.

## How `esp_at` works (reference)

| Piece | Role |
|-------|------|
| DTS | UART child `compatible = "espressif,esp-at"` (+ reset/power GPIOs) |
| Kconfig | `CONFIG_WIFI_ESP_AT` → `MODEM_*`, `WIFI_OFFLOAD`, `NET_L2_WIFI_MGMT` |
| Driver | Modem cmd handler speaks ESP-AT; implements `wifi_mgmt_ops` + `net_offload` |
| App API | `net_mgmt(NET_REQUEST_WIFI_CONNECT, …)` and BSD sockets — **not** raw AT |

Closest cousins: `inventek,eswifi-uart` (same offload model, different AT set).

## What a GS1500M driver would look like

1. **DTS binding** e.g. `gainspan,gs1500m-at` on `uart0`, with `reset-gpios`, `pgm-gpios`, `intf-sel-gpios` (board already has `wifi-reset` / `wifi-pgm` / `wifi-intf-sel` aliases).
2. **Kconfig** `WIFI_GS1500M_AT` selecting the same modem + offload stack as esp_at.
3. **Driver** (prefer reuse of `lib/wifi/gs1500m` AT parser over rewriting ESP-AT strings):
   - RX thread / workqueue (esp_at style)
   - `wifi_mgmt_ops`: scan (optional), connect/disconnect, iface status
   - `net_offload` / sockets: map to GainSpan `AT+NCTCP` / `AT+NCUDP` + ESC Z/Y
4. **App**: drop `gs_user_poll`; use Zephyr net samples / `wifi` shell.
5. **West**: expand `west.yml` allowlist for networking modules (`net`, etc.) — currently omitted.

## Effort / risks

- GainSpan Serial2WiFi ≠ ESP-AT command set (join, DHCP, TLS, SNTP are different).
- Socket offload + SSL/MQTT currently live in the portable lib; they must be re-homed or kept as a higher layer.
- CI/west must pull net modules; flash/RAM on MK24 need checking.

## Suggested sequence

1. ~~Zephyr native `LOG_*` + `SYS_CLOCK_REALTIME`~~ (this change)
2. Keep portable AT lib for FreeRTOS + Zephyr bring-up until offload is green
3. Add OOT module `zephyr/drivers/wifi/gs1500m/` with DTS + `wifi_mgmt` connect only
4. Add socket offload
5. Point `apps/zephyr_wifi` at `net_mgmt` and retire `gs_user_*` on Zephyr
