/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PIN_MUX_H_
#define _PIN_MUX_H_

#include "fsl_common.h"

#define BOARD_LED_GPIO     GPIOA
#define BOARD_LED_GPIO_PIN 29U

/* GS1500M — see docs/gs1500m_pins.md */
#define BOARD_WIFI_UART_RX_PIN   6U  /* PTD6 */
#define BOARD_WIFI_UART_TX_PIN   7U  /* PTD7 */
#define BOARD_WIFI_RESET_PIN     5U  /* PTD5, active-low */
#define BOARD_WIFI_PGM_PIN       6U  /* PTE6 */
#define BOARD_WIFI_INTF_SEL_PIN  11U /* PTA11 */
#define BOARD_WIFI_RTC_OUT_PIN   16U /* PTB16 */
#define BOARD_WIFI_ALARM1_PIN    9U  /* PTD9 */
#define BOARD_WIFI_SPI_IRQ_PIN   10U /* PTD10 (reserved) */

#if defined(__cplusplus)
extern "C" {
#endif

void BOARD_InitBootPins(void);
void BOARD_InitPins(void);

#if defined(__cplusplus)
}
#endif

#endif /* _PIN_MUX_H_ */
