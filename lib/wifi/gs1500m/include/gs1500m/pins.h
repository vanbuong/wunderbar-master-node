/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * WunderBar GS1500M pin definitions (MK24FN1M0VDC12).
 * See docs/gs1500m_pins.md for schematic context.
 */

#ifndef GS1500M_PINS_H
#define GS1500M_PINS_H

/* UART0 AT path */
#define GS_PIN_UART_RX_PORT   D
#define GS_PIN_UART_RX_NUM    6U /* PTD6 — WIFI_UART_TX_LPC_RX */
#define GS_PIN_UART_TX_PORT   D
#define GS_PIN_UART_TX_NUM    7U /* PTD7 — WIFI_UART_RX_LPC_TX */

#define GS_UART_BAUD_DEFAULT  115200U

/* Control GPIOs */
#define GS_PIN_RESET_PORT     D
#define GS_PIN_RESET_NUM      5U /* PTD5 — WIFI_!RESET (active low) */
#define GS_PIN_PGM_PORT       E
#define GS_PIN_PGM_NUM        6U /* PTE6 — WIFI_PGM */
#define GS_PIN_INTF_SEL_PORT  A
#define GS_PIN_INTF_SEL_NUM   11U /* PTA11 — WIFI_INTF_SEL */

/* Optional inputs */
#define GS_PIN_RTC_OUT_PORT   B
#define GS_PIN_RTC_OUT_NUM    16U /* PTB16 */
#define GS_PIN_ALARM1_PORT    D
#define GS_PIN_ALARM1_NUM     9U  /* PTD9 */
#define GS_PIN_SPI_IRQ_PORT   D
#define GS_PIN_SPI_IRQ_NUM    10U /* PTD10 — reserved */

/* SPI reserved (AT path does not use these) */
#define GS_PIN_SPI_SSEL_NUM   11U /* PTD11 */
#define GS_PIN_SPI_SCK_NUM    12U /* PTD12 */
#define GS_PIN_SPI_MOSI_NUM   13U /* PTD13 */
#define GS_PIN_SPI_MISO_NUM   14U /* PTD14 */

/*
 * Drive level that selects UART mode on INTF_SEL.
 * Flip to 1 if a board revision uses the opposite sense.
 */
#ifndef GS_INTF_SEL_UART_LEVEL
#define GS_INTF_SEL_UART_LEVEL 0U
#endif

/* PGM idle (normal boot) level — deasserted high on this PCB. */
#ifndef GS_PGM_IDLE_LEVEL
#define GS_PGM_IDLE_LEVEL 1U
#endif

#endif /* GS1500M_PINS_H */
