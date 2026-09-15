# GS1500M pin map (WunderBar master / MK24)

UART AT path (primary — matches legacy `WunderBar_WiFi` firmware). SPI pins are reserved and unused by this rewrite.

| Signal | MK24 pin | Role |
|--------|----------|------|
| `WIFI_UART_TX_LPC_RX` | **PTD6** | MCU UART0 RX |
| `WIFI_UART_RX_LPC_TX` | **PTD7** | MCU UART0 TX |
| `WIFI_!RESET` | **PTD5** | Active-low reset (open-drain pulse; module ready when released) |
| `WIFI_PGM` | **PTE6** | Programming mode — keep **deasserted** for normal boot |
| `WIFI_INTF_SEL` | **PTA11** | Interface select — drive UART mode at boot |
| `WIFI_RTC_OUT` | **PTB16** | Module RTC out (input, optional) |
| `WIFI_ALARM1` | **PTD9** | Alarm GPIO (input) |
| `WIFI_SPI_IRQ` | **PTD10** | SPI IRQ (unused for AT v1; GPIO reserved) |
| SPI SSEL/SCK/MOSI/MISO | **PTD11–14** | Reserved; not used by AT UART path |

**Framing:** UART0, **115200 8N1**, no hardware flow control.

**INTF_SEL polarity:** default UART level is `0` (`GS_INTF_SEL_UART_LEVEL`). Flip via compile define / DTS property if a PCB revision requires it.
