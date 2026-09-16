/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr GS1500M WiFi bring-up + optional STA join.
 * Logging: Zephyr LOG_*; wall clock: SYS_CLOCK_REALTIME (via wb_time_zephyr).
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/clock.h>

#include <string.h>
#include <time.h>

#include "gs1500m/wifi.h"
#include "gs1500m/user.h"
#include "gs_platform_zephyr.h"
#include "wb_wifi_cred.h"
#include "wb_time.h"

LOG_MODULE_REGISTER(wb_wifi, LOG_LEVEL_INF);

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
	case GS_USER_HTTP_TIME: return "HTTP_TIME";
	case GS_USER_LOAD_CA: return "LOAD_CA";
	case GS_USER_READY: return "READY";
	case GS_USER_ERROR: return "ERROR";
	case GS_USER_LIMITED_AP: return "LIMITED_AP";
	case GS_USER_MQTT: return "MQTT";
	default: return "?";
	}
}

static void log_module_info(void)
{
	const gs_wifi_module_info_t *mi = gs_wifi_last_module_info();

	if (!mi) {
		return;
	}
	LOG_INF("wifi module: name=%s mac=%s",
		mi->name[0] ? mi->name : "(unknown)",
		mi->mac[0] ? mi->mac : "(unknown)");
	if (mi->app_ver[0] || mi->geps_ver[0] || mi->wlan_ver[0]) {
		LOG_INF("wifi fw: app=%s geps=%s wlan=%s",
			mi->app_ver[0] ? mi->app_ver : "?",
			mi->geps_ver[0] ? mi->geps_ver : "?",
			mi->wlan_ver[0] ? mi->wlan_ver : "?");
	} else if (mi->version[0]) {
		LOG_INF("wifi fw: %s", mi->version);
	} else {
		LOG_INF("wifi fw: (unavailable)");
	}
}

static void log_ip(void)
{
	const char *ip = gs_wifi_last_ip();
	char buf[16];

	if (!ip || !ip[0]) {
		if (gs_wifi_get_ip(buf, sizeof(buf)) == GS_MSG_OK) {
			ip = buf;
		}
	}
	LOG_INF("wifi ip: %s", (ip && ip[0]) ? ip : "(none)");
}

static void log_ntp_time(void)
{
	const char *t = gs_wifi_last_time_str();
	struct timespec ts;

	(void)sys_clock_gettime(SYS_CLOCK_REALTIME, &ts);
	LOG_INF("wifi ntp: %s (unix=%u synced=%d realtime=%lld)",
		(t && t[0]) ? t : "(sync failed)",
		(unsigned)gs_wifi_last_unix_time(),
		wb_time_is_synced() ? 1 : 0,
		(long long)ts.tv_sec);
}

int main(void)
{
	gs_user_config_t cfg;
	gs_user_state_t prev = GS_USER_IDLE;

	/* Bridge GS NTP → SYS_CLOCK_REALTIME (see wb_time_zephyr). */
	wb_time_init(NULL, NULL);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}
	(void)gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

#if CONSOLE_IS_USB_CDC
	wait_for_dtr();
	LOG_INF("WunderBar WiFi (Zephyr USB)");
#else
	LOG_INF("WunderBar WiFi (Zephyr RTT)");
#endif

	if (gs_platform_zephyr_init(&s_gs_plat) != 0) {
		LOG_ERR("GS platform init failed");
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
		LOG_INF("cred @0x%08X raw=%02X%02X%02X%02X%02X%02X%02X%02X",
			(unsigned)WB_WIFI_CRED_FLASH_ADDR,
			(unsigned)(uint8_t)c->magic[0], (unsigned)(uint8_t)c->magic[1],
			(unsigned)(uint8_t)c->magic[2], (unsigned)(uint8_t)c->magic[3],
			(unsigned)(uint8_t)c->magic[4], (unsigned)(uint8_t)c->magic[5],
			(unsigned)(uint8_t)c->magic[6], (unsigned)(uint8_t)c->magic[7]);
		LOG_INF("cred valid=%d ssid=%s",
			wb_wifi_cred_valid() ? 1 : 0,
			(cfg.ssid && cfg.ssid[0]) ? cfg.ssid : "(none)");
	}
	LOG_INF("GS1500M bring-up (ssid %s)",
		(cfg.ssid && cfg.ssid[0]) ? cfg.ssid : "(none)");

	while (1) {
		gs_user_state_t st = gs_user_poll(&s_user);
		if (st != prev) {
			LOG_INF("wifi SM: %s (msg=%d)", state_name(st),
				(int)s_user.last_msg);
			if (prev == GS_USER_INIT && st != GS_USER_ERROR) {
				const gs_wifi_init_diag_t *d = gs_wifi_last_init_diag();
				if (d) {
					LOG_INF("wifi link: baud=%u intf=%s pgm=%s hw_rst=%d saw_boot=%d rx=%u",
						(unsigned)d->baud,
						d->intf_sel < 0 ? "float" :
							(d->intf_sel ? "1" : "0"),
						d->pgm_idle < 0 ? "float" :
							(d->pgm_idle ? "1" : "0"),
						d->hw_reset ? 1 : 0,
						d->saw_boot ? 1 : 0,
						(unsigned)d->rx_bytes);
				}
			}
			if (st == GS_USER_HTTP_TIME) {
				log_module_info();
				log_ip();
			}
			if (prev == GS_USER_HTTP_TIME && st != GS_USER_ERROR) {
				log_ntp_time();
			}
			if (st == GS_USER_ERROR) {
				const gs_wifi_init_diag_t *d = gs_wifi_last_init_diag();
				LOG_ERR("last AT line: '%s'", gs_at_last_line());
				LOG_ERR("AT rx_bytes=%u partial='%s'",
					(unsigned)gs_at_rx_byte_count(),
					gs_at_partial_line()[0] ? gs_at_partial_line()
								: "");
				if (d) {
					LOG_ERR("init try baud=%u intf=%s pgm=%s hw_rst=%d saw_boot=%d rx=%u",
						(unsigned)d->baud,
						d->intf_sel < 0 ? "float" :
							(d->intf_sel ? "1" : "0"),
						d->pgm_idle < 0 ? "float" :
							(d->pgm_idle ? "1" : "0"),
						d->hw_reset ? 1 : 0,
						d->saw_boot ? 1 : 0,
						(unsigned)d->rx_bytes);
				}
			}
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
