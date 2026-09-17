# GS1500M as a Zephyr WiFi offload driver (esp_at pattern)

## Status

**wifi_mgmt + IPv4 TCP/UDP `net_offload`** under `drivers/wifi/gs1500m/`.
Association, iface status, module NTP → `SYS_CLOCK_REALTIME`, and Zephyr
native sockets (TCP/UDP client) work through `lib/wifi/gs1500m`.

Listen/accept (TCP server) and TLS socket offload are not implemented yet;
use host mbedTLS on TCP, or module `gs_ssl_*` / `gs_http_*` / `gs_mqtt_pipe_*`.

## Layout

| Path | Role |
|------|------|
| `dts/bindings/wifi/gainspan,gs1500m-at.yaml` | DTS binding |
| `drivers/wifi/gs1500m/` | Driver (`wifi_mgmt` + `net_offload`) |
| `zephyr/module.yml` | Registers cmake/kconfig/dts_root |
| `apps/zephyr_wifi/app.overlay` | Sets `&gs1500m { status = "okay"; }` |
| `lib/wifi/gs1500m/` | Shared AT library (also FreeRTOS) |

## App API

```c
/* Associate */
net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(cnx_params));

/* Zephyr-native sockets (IPv4 TCP/UDP client) */
int fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
zsock_connect(fd, ...);
zsock_send(fd, ...);
```

Under the hood: `AT+NCTCP` / `AT+NCUDP` + ESC Z bulk TX + ESC RX → `net_pkt`.

## Next milestones

1. TLS via host mbedTLS (or `AT+SSLOPEN` offload)
2. TCP server (`listen`/`accept`)
3. Scan / AP ops
4. Upstream to Zephyr (optional)
