/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * WunderBar master blinky — LED on PTA29 via Zephyr GPIO.
 * Logs go to USB CDC ACM (host /dev/ttyACM* or COMx).
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#define SLEEP_TIME_MS 500
#define DTR_WAIT_MS 5000

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_EXISTS(LED0_NODE)
#error "Overlay/board must define alias led0 (PTA29)"
#endif

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static void wait_for_dtr(void)
{
	const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int waited = 0;

	if (!device_is_ready(cons)) {
		return;
	}

	/* Don't block forever: LED must still blink if no host is attached. */
	while (!dtr && waited < DTR_WAIT_MS) {
		(void)uart_line_ctrl_get(cons, UART_LINE_CTRL_DTR, &dtr);
		k_msleep(100);
		waited += 100;
	}
}

int main(void)
{
	int ret;
	bool on = false;

	if (!gpio_is_ready_dt(&led)) {
		printf("LED GPIO device not ready\n");
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printf("Failed to configure LED (%d)\n", ret);
		return 0;
	}

	wait_for_dtr();
	printf("WunderBar blinky on PTA29 (Zephyr)\n");

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		on = !on;
		printf("LED %s\n", on ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}

	return 0;
}
