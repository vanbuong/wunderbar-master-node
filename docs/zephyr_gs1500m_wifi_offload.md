# GS1500M as a Zephyr WiFi offload driver (esp_at pattern)

## Status

**v1 wifi_mgmt offload is in-tree (OOT module)** under `drivers/wifi/gs1500m/`.
Socket `net_offload` is stubbed (`-ENOTSUP`); association + iface status +
module NTP → `SYS_CLOCK_REALTIME` work through `lib/wifi/gs1500m`.

## Layout

| Path | Role |
|------|------|
| `dts/bindings/wifi/gainspan,gs1500m-at.yaml` | DTS binding |
| `drivers/wifi/gs1500m/` | Driver (`wifi_mgmt` + stub offload) |
| `zephyr/module.yml` | Registers cmake/kconfig/dts_root |
| `apps/zephyr_wifi/app.overlay` | Sets `&gs1500m { status = "okay"; }` |
| `lib/wifi/gs1500m/` | Shared AT library (also FreeRTOS) |

## App API

```c
net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(cnx_params));
net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));
```

## Next milestones

1. Real `net_offload` / sockets (`AT+NCTCP` / ESC Z)
2. Scan / AP ops
3. Upstream to Zephyr (optional)
