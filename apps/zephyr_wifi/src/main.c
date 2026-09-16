/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr GS1500M WiFi bring-up + optional STA join.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <string.h>

#include "wb_log.h"
#include "gs1500m/wifi.h"
#include "gs1500m/user.h"
#include "gs_platform_zephyr.h"
#include "wb_wifi_cred.h"

#if DT_HAS_CHOSEN(zephyr_console) && \
	DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
#define CONSOLE_IS_USB_CDC 1
#include <zephyr/drivers/uart.h>
#else
#define CONSOLE_IS_USB_CDC 0
#endif

#ifndef CONFIG_WB_WIFI_SSID
#define CONFIG_WB_WIFI_SSID ""
#endif
#ifndef CONFIG_WB_WIFI_PSK
#define CONFIG_WB_WIFI_PSK ""
#endif

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_EXISTS(LED0_NODE)
#error "Board must define alias led0 (PTA29)"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static gs_platform_t s_gs_plat;
static gs_user_t s_user;

#if CONSOLE_IS_USB_CDC
static void wait_for_dtr(void)
{
	const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int waited = 0;

	if (!device_is_ready(cons)) {
		return;
	}
	while (!dtr && waited < 5000) {
		(void)uart_line_ctrl_get(cons, UART_LINE_CTRL_DTR, &dtr);
		k_msleep(100);
		waited += 100;
	}
}
#endif

static const char *state_name(gs_user_state_t st)
{
	switch (st) {
	case GS_USER_IDLE: return "IDLE";
	case GS_USER_INIT: return "INIT";
	case GS_USER_JOIN: return "JOIN";
	case GS_USER_READY: return "READY";
	case GS_USER_ERROR: return "ERROR";
	case GS_USER_LIMITED_AP: return "LIMITED_AP";
	case GS_USER_MQTT: return "MQTT";
	default: return "?";
	}
}

int main(void)
{
	gs_user_config_t cfg;
	gs_user_state_t prev = GS_USER_IDLE;

	wb_log_init(wb_log_stdio_backend(), WB_LOG_INFO);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}
	(void)gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

#if CONSOLE_IS_USB_CDC
	wait_for_dtr();
	WB_LOGI("WunderBar WiFi (Zephyr USB)");
#else
	WB_LOGI("WunderBar WiFi (Zephyr RTT)");
#endif

	if (gs_platform_zephyr_init(&s_gs_plat) != 0) {
		WB_LOGE("GS platform init failed");
		return 0;
	}

	memset(&cfg, 0, sizeof(cfg));
	if (wb_wifi_cred_ssid()) {
		cfg.ssid = wb_wifi_cred_ssid();
		cfg.psk = wb_wifi_cred_psk() ? wb_wifi_cred_psk() : "";
	} else {
		cfg.ssid = CONFIG_WB_WIFI_SSID;
		cfg.psk = CONFIG_WB_WIFI_PSK;
	}
	gs_user_init(&s_user, &cfg);

	{
		const wb_wifi_cred_t *c = wb_wifi_cred_at_flash();
		WB_LOGI("cred @0x%08X raw=%02X%02X%02X%02X%02X%02X%02X%02X",
			(unsigned)WB_WIFI_CRED_FLASH_ADDR,
			(unsigned)(uint8_t)c->magic[0], (unsigned)(uint8_t)c->magic[1],
			(unsigned)(uint8_t)c->magic[2], (unsigned)(uint8_t)c->magic[3],
			(unsigned)(uint8_t)c->magic[4], (unsigned)(uint8_t)c->magic[5],
			(unsigned)(uint8_t)c->magic[6], (unsigned)(uint8_t)c->magic[7]);
		WB_LOGI("cred valid=%d ssid=%s",
			wb_wifi_cred_valid() ? 1 : 0,
			(cfg.ssid && cfg.ssid[0]) ? cfg.ssid : "(none)");
	}
	WB_LOGI("GS1500M bring-up (ssid %s)",
		(cfg.ssid && cfg.ssid[0]) ? cfg.ssid : "(none)");

	while (1) {
		gs_user_state_t st = gs_user_poll(&s_user);
		if (st != prev) {
			WB_LOGI("wifi SM: %s (msg=%d)", state_name(st),
				(int)s_user.last_msg);
			prev = st;
		}

		if (st == GS_USER_READY || st == GS_USER_ERROR) {
			gs_platform_zephyr_rx_poll(50);
			(void)gpio_pin_toggle_dt(&led);
			k_msleep(500);
		} else {
			gs_platform_zephyr_rx_poll(10);
			k_msleep(20);
		}
	}

	return 0;
}
