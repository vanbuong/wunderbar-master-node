/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * WunderBar master PCB — nRF51822 QFAA pin aliases.
 */

#ifndef WB_NRF51_BOARD_H
#define WB_NRF51_BOARD_H

#include "nrf.h"
#include "nrf_clock.h"

/* SPI slave toward MK24 */
#define WB_PIN_SPIS_MISO   0   /* P0.00 */
#define WB_PIN_SPIS_MOSI   1   /* P0.01 */
#define WB_PIN_SPIS_CSN    3   /* P0.03 */
#define WB_PIN_SPIS_SCK    5   /* P0.05 */

/* Host handshake */
#define WB_PIN_READY_GP1   2   /* P0.02 — assert high when TX frame queued */
#define WB_PIN_CTRL_GP2    4   /* P0.04 — reserved (legacy bootloader enter) */

/* Status LED (active high) */
#define WB_PIN_LED         29  /* P0.29 */

/* LF crystal on P0.26/P0.27 (see docs/nrf51822_pins.md). */
#define NRF_CLOCK_LFCLKSRC                                                         \
	{                                                                          \
		.source = NRF_CLOCK_LF_SRC_XTAL, .rc_ctiv = 0, .rc_temp_ctiv = 0,  \
		.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM                 \
	}

#define WB_LED_ON()   do { NRF_GPIO->OUTSET = (1u << WB_PIN_LED); } while (0)
#define WB_LED_OFF()  do { NRF_GPIO->OUTCLR = (1u << WB_PIN_LED); } while (0)
#define WB_LED_TOGGLE() do { NRF_GPIO->OUT ^= (1u << WB_PIN_LED); } while (0)

#define WB_READY_ASSERT()   do { NRF_GPIO->OUTSET = (1u << WB_PIN_READY_GP1); } while (0)
#define WB_READY_DEASSERT() do { NRF_GPIO->OUTCLR = (1u << WB_PIN_READY_GP1); } while (0)

#endif /* WB_NRF51_BOARD_H */
