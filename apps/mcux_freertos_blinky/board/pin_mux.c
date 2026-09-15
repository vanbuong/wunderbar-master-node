/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PTA29 GPIO as LED (PORTA pin 29).
 */

#include "fsl_common.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

void BOARD_InitBootPins(void)
{
    BOARD_InitPins();
}

void BOARD_InitPins(void)
{
    CLOCK_EnableClock(kCLOCK_PortA);
    PORT_SetPinMux(PORTA, BOARD_LED_GPIO_PIN, kPORT_MuxAsGpio);
}
