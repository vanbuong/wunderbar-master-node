/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * GainSpan GS1500M WiFi offload — private driver state.
 */

#ifndef ZEPHYR_DRIVERS_WIFI_GS1500M_GS_H_
#define ZEPHYR_DRIVERS_WIFI_GS1500M_GS_H_

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/drivers/gpio.h>

#include "gs1500m/wifi.h"
#include "gs_platform_zephyr.h"

#define GS1500M_MTU 1500

struct gs1500m_config {
	const struct gpio_dt_spec reset;
	const struct gpio_dt_spec pgm;
	const struct gpio_dt_spec intf_sel;
	uint32_t target_speed;
};

struct gs1500m_data {
	struct net_if *iface;
	gs_platform_t plat;

	struct k_mutex lock;
	struct k_work_q workq;
	struct k_work init_work;
	struct k_work connect_work;
	struct k_work disconnect_work;

	struct wifi_connect_req_params conn;
	char ssid[WIFI_SSID_MAX_LEN + 1];
	char psk[64];

	bool initialized;
	bool connected;
	bool connecting;

	struct k_thread rx_thread;
	k_thread_stack_t *rx_stack;
};

int gs1500m_offload_init(struct net_if *iface);

#endif /* ZEPHYR_DRIVERS_WIFI_GS1500M_GS_H_ */
