/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * PTA29 LED + GS1500M UART0 (PTD6/7) and control GPIOs.
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
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortD);
    CLOCK_EnableClock(kCLOCK_PortE);

    /* User LED */
    PORT_SetPinMux(PORTA, BOARD_LED_GPIO_PIN, kPORT_MuxAsGpio);

    /* GS1500M UART0: PTD6=RX (ALT3), PTD7=TX (ALT3) */
    PORT_SetPinMux(PORTD, BOARD_WIFI_UART_RX_PIN, kPORT_MuxAlt3);
    PORT_SetPinMux(PORTD, BOARD_WIFI_UART_TX_PIN, kPORT_MuxAlt3);

    /* GS1500M control / status GPIOs */
    PORT_SetPinMux(PORTD, BOARD_WIFI_RESET_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTE, BOARD_WIFI_PGM_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTA, BOARD_WIFI_INTF_SEL_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTB, BOARD_WIFI_RTC_OUT_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTD, BOARD_WIFI_ALARM1_PIN, kPORT_MuxAsGpio);
    PORT_SetPinMux(PORTD, BOARD_WIFI_SPI_IRQ_PIN, kPORT_MuxAsGpio);
}
