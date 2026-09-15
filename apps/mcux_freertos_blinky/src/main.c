/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOS blinky for WunderBar master — LED on PTA29.
 */

#include "board.h"
#include "fsl_gpio.h"

#include "FreeRTOS.h"
#include "task.h"

#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 1
#endif

static void prvLedInit(void)
{
    gpio_pin_config_t cfg = {
        .pinDirection = kGPIO_DigitalOutput,
#if LED_ACTIVE_HIGH
        .outputLogic = 0U,
#else
        .outputLogic = 1U,
#endif
    };

    GPIO_PinInit(BOARD_LED_GPIO, BOARD_LED_GPIO_PIN, &cfg);
}

static void prvBlinkTask(void *pvParameters)
{
    (void)pvParameters;
    prvLedInit();

    for (;;) {
        GPIO_PortToggle(BOARD_LED_GPIO, 1U << BOARD_LED_GPIO_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    BOARD_InitHardware();

    if (xTaskCreate(prvBlinkTask, "blink", configMINIMAL_STACK_SIZE + 64, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        for (;;) {
        }
    }

    vTaskStartScheduler();

    for (;;) {
    }
}
