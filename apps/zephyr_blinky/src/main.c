/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * WunderBar master blinky — LED on PTA29 via Zephyr GPIO.
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define SLEEP_TIME_MS 500

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_EXISTS(LED0_NODE)
#error "Overlay/board must define alias led0 (PTA29)"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

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
