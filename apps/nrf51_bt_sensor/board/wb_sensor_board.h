/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * IMU sensor board (schematic): nRF51822 + Invensense MPU-6xxx over I2C.
 */

#ifndef WB_SENSOR_BOARD_H
#define WB_SENSOR_BOARD_H

#include "nrf.h"
#include "nrf_clock.h"

/* I2C ↔ IMU */
#define WB_PIN_GYRO_SCL  20  /* P0.20 */
#define WB_PIN_GYRO_SDA  21  /* P0.21 */
#define WB_PIN_GYRO_INT  22  /* P0.22 */

/*
 * AD0 pulled high via R19 on this PCB → 7-bit address 0x69.
 * WHO_AM_I probe also tries 0x68 if 0x69 fails.
 */
#define WB_IMU_ADDR_PRIMARY   0x69u
#define WB_IMU_ADDR_FALLBACK  0x68u

/* User UI — LED is active-low (cathode toward MCU). */
#define WB_PIN_BTN2  28  /* P0.28, active-low to GND */
#define WB_PIN_LED   29  /* P0.29 */

#define WB_LED_ON()    do { NRF_GPIO->OUTCLR = (1u << WB_PIN_LED); } while (0)
#define WB_LED_OFF()   do { NRF_GPIO->OUTSET = (1u << WB_PIN_LED); } while (0)
#define WB_LED_TOGGLE() do { NRF_GPIO->OUT ^= (1u << WB_PIN_LED); } while (0)

#define NRF_CLOCK_LFCLKSRC                                                         \
	{                                                                          \
		.source = NRF_CLOCK_LF_SRC_XTAL, .rc_ctiv = 0, .rc_temp_ctiv = 0,  \
		.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM                 \
	}

#endif /* WB_SENSOR_BOARD_H */
