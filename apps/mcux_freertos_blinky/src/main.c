/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOS blinky for WunderBar master — LED on PTA29.
 * Logs via wb_log → stdout → USB CDC or SEGGER RTT (_write).
 */

#include <stdio.h>

#include "board.h"
#include "fsl_gpio.h"
#include "wb_log.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef LOG_BACKEND_RTT
#include "SEGGER_RTT.h"
#else
#include "tusb.h"
#endif

#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 1
#endif

#define USBD_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)
#define BLINK_STACK_SIZE (configMINIMAL_STACK_SIZE + 128)

static void prvLedInit(void)
{
    gpio_pin_config_t cfg = {
        .pinDirection = kGPIO_DigitalOutput,
#if LED_ACTIVE_HIGH
        .outputLogic = 1U,
#else
        .outputLogic = 0U,
#endif
    };

    GPIO_PinInit(BOARD_LED_GPIO, BOARD_LED_GPIO_PIN, &cfg);
}

static void prvLedToggle(void)
{
    GPIO_PortToggle(BOARD_LED_GPIO, 1U << BOARD_LED_GPIO_PIN);
}

static void prvBusyBlinkForever(void)
{
    volatile uint32_t i;
    for (;;) {
        prvLedToggle();
        for (i = 0; i < 800000U; i++) {
        }
    }
}

int _write(int fd, char *ptr, int len)
{
    if ((fd != 1) && (fd != 2)) {
        return -1;
    }
    if ((ptr == NULL) || (len <= 0)) {
        return 0;
    }

#ifdef LOG_BACKEND_RTT
    return (int)SEGGER_RTT_Write(0, ptr, (unsigned)len);
#else
    {
        int n = 0;

        if (!tud_inited() || !tud_cdc_connected()) {
            return len;
        }

        while (n < len) {
            uint32_t avail = tud_cdc_write_available();
            uint32_t chunk;

            if (avail == 0U) {
                tud_cdc_write_flush();
                if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
                    vTaskDelay(1);
                }
                if (!tud_cdc_connected()) {
                    break;
                }
                continue;
            }

            chunk = (uint32_t)(len - n);
            if (chunk > avail) {
                chunk = avail;
            }
            n += (int)tud_cdc_write((uint8_t const *)ptr + n, chunk);
            tud_cdc_write_flush();
        }

        return (n > 0) ? n : len;
    }
#endif
}

#ifndef LOG_BACKEND_RTT
static void prvUsbTask(void *pvParameters)
{
    (void)pvParameters;

    tud_init(BOARD_TUD_RHPORT);

    for (;;) {
        tud_task();
        tud_cdc_write_flush();
    }
}

void tud_mount_cb(void)
{
}

void tud_umount_cb(void)
{
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
}

void tud_resume_cb(void)
{
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void)itf;
    (void)rts;

    if (dtr) {
        WB_LOGI("WunderBar blinky on PTA29 (FreeRTOS USB)");
    }
}
#endif

static void prvBlinkTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;) {
        prvLedToggle();
#ifdef LOG_BACKEND_RTT
        WB_LOGI("LED toggle (FreeRTOS RTT)");
#else
        WB_LOGI("LED toggle (FreeRTOS USB)");
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    BOARD_InitHardware();
    prvLedInit();
    wb_log_init(wb_log_stdio_backend(), WB_LOG_INFO);

#ifdef LOG_BACKEND_RTT
    SEGGER_RTT_Init();
    WB_LOGI("WunderBar blinky on PTA29 (FreeRTOS RTT)");
#else
    if (xTaskCreate(prvUsbTask, "usb", USBD_STACK_SIZE, NULL,
                    configMAX_PRIORITIES - 1, NULL) != pdPASS) {
        prvBusyBlinkForever();
    }
#endif

    if (xTaskCreate(prvBlinkTask, "blink", BLINK_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        prvBusyBlinkForever();
    }

    vTaskStartScheduler();
    prvBusyBlinkForever();
}
