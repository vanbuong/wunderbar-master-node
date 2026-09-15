/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * WunderBar master blinky — LED on PTA29 via Zephyr GPIO.
 * Logs via wb_log → stdout → USB CDC ACM or SEGGER RTT console.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "wb_log.h"

#if DT_HAS_CHOSEN(zephyr_console) && \
	DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
#define CONSOLE_IS_USB_CDC 1
#include <zephyr/drivers/uart.h>
#else
#define CONSOLE_IS_USB_CDC 0
#endif

#define SLEEP_TIME_MS 500
#define DTR_WAIT_MS 5000

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_EXISTS(LED0_NODE)
#error "Overlay/board must define alias led0 (PTA29)"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

#if CONSOLE_IS_USB_CDC
static void wait_for_dtr(void)
{
	const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int waited = 0;

	if (!device_is_ready(cons)) {
		return;
	}

	while (!dtr && waited < DTR_WAIT_MS) {
		(void)uart_line_ctrl_get(cons, UART_LINE_CTRL_DTR, &dtr);
		k_msleep(100);
		waited += 100;
	}
}
#endif

int main(void)
{
	int ret;
	bool on = false;

	wb_log_init(wb_log_stdio_backend(), WB_LOG_INFO);

	if (!gpio_is_ready_dt(&led)) {
		WB_LOGE("LED GPIO device not ready");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		WB_LOGE("Failed to configure LED (%d)", ret);
		return 0;
	}

#if CONSOLE_IS_USB_CDC
	wait_for_dtr();
	WB_LOGI("WunderBar blinky on PTA29 (Zephyr USB)");
#else
	WB_LOGI("WunderBar blinky on PTA29 (Zephyr RTT)");
#endif

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		on = !on;
		WB_LOGI("LED %s", on ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
