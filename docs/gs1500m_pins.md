# GS1500M pin map (WunderBar master / MK24)

UART AT path (primary — matches legacy `WunderBar_WiFi` firmware). SPI pins are reserved and unused by this rewrite.

| Signal | MK24 pin | Role |
|--------|----------|------|
| `WIFI_UART_TX_LPC_RX` | **PTD6** | MCU UART0 RX |
| `WIFI_UART_RX_LPC_TX` | **PTD7** | MCU UART0 TX |
| `WIFI_!RESET` | **PTD5** | PE: GPIO **input** idle (hi-Z); pulse = drive low then release to input |
| `WIFI_PGM` | **PTE6** | PE: **not initialized** — leave floating (board pull = run). High at reset ⇒ flash-download |
| `WIFI_INTF_SEL` | **PTA11** | PE: mux GPIO then leave alone — bring-up tries float / 1 / 0 |
| `WIFI_RTC_OUT` | **PTB16** | Module RTC out (input, optional) |
| `WIFI_ALARM1` | **PTD9** | Alarm GPIO (input) |
| `WIFI_SPI_IRQ` | **PTD10** | SPI IRQ (unused for AT v1; GPIO reserved) |
| SPI SSEL/SCK/MOSI/MISO | **PTD11–14** | Reserved; not used by AT UART path |

**Framing:** UART0, **115200 8N1**, no hardware flow control.

**INTF_SEL polarity:** bring-up tries **float (legacy)**, then **1**, then **0**. GainSpan UART is typically high / SPI low; Serial2WiFi firmware may ignore the pin. Override `GS_INTF_SEL_UART_LEVEL` only if you need a fixed drive level outside auto-try.

**Baud:** factory default is often **9600**. Init tries 115200, then 9600, then 57600. Framing-error bytes are kept (not discarded) so a baud mismatch still shows `rx_bytes>0`. After a 9600 link, firmware issues `ATB=115200` and retunes the host UART.

**PGM:** GainSpan samples UART1_RTS/GPIO27 at reset — **high = programming / flash-download**, **low = run Serial2WiFi**. Default idle is `0`. Bring-up also retries the opposite polarity.

