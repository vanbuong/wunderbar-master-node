/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOS GS1500M WiFi bring-up + optional STA join (WB_WIFI_SSID/PSK).
 */

#include <stdio.h>
#include <string.h>

#include "board.h"
#include "fsl_gpio.h"
#include "fsl_clock.h"
#include "fsl_uart.h"
#include "wb_log.h"
#include "wb_time.h"

#include "gs1500m/wifi.h"
#include "gs1500m/user.h"
#include "gs1500m/pins.h"
#include "gs_platform_freertos.h"
#include "wb_wifi_cred.h"

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

#ifndef WB_WIFI_SSID
#define WB_WIFI_SSID ""
#endif
#ifndef WB_WIFI_PSK
#define WB_WIFI_PSK ""
#endif

#define USBD_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)
#define WIFI_STACK_SIZE (configMINIMAL_STACK_SIZE * 6)

static gs_platform_t s_gs_plat;
static gs_user_t s_user;

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

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
	(void)itf;
	(void)rts;
	if (dtr) {
		WB_LOGI("WunderBar WiFi (FreeRTOS USB)");
	}
}
#endif

static const char *prvMsgName(gs_msg_id_t id)
{
	switch (id) {
	case GS_MSG_NONE: return "NONE";
	case GS_MSG_OK: return "OK";
	case GS_MSG_ERROR: return "ERROR";
	case GS_MSG_TIMEOUT: return "TIMEOUT";
	case GS_MSG_WELCOME: return "WELCOME";
	case GS_MSG_APP_RESET: return "APP_RESET";
	case GS_MSG_CONNECT: return "CONNECT";
	default: return "?";
	}
}

static const char *prvStateName(gs_user_state_t st)
{
	switch (st) {
	case GS_USER_IDLE: return "IDLE";
	case GS_USER_INIT: return "INIT";
	case GS_USER_JOIN: return "JOIN";
	case GS_USER_HTTP_TIME: return "HTTP_TIME";
	case GS_USER_LOAD_CA: return "LOAD_CA";
	case GS_USER_MQTT: return "MQTT";
	case GS_USER_LIMITED_AP: return "LIMITED_AP";
	case GS_USER_READY: return "READY";
	case GS_USER_ERROR: return "ERROR";
	default: return "?";
	}
}

static void prvLogModuleInfo(void)
{
	const gs_wifi_module_info_t *mi = gs_wifi_last_module_info();

	if (!mi) {
		return;
	}
	WB_LOGI("wifi module: name=%s mac=%s",
		mi->name[0] ? mi->name : "(unknown)",
		mi->mac[0] ? mi->mac : "(unknown)");
	if (mi->app_ver[0] || mi->geps_ver[0] || mi->wlan_ver[0]) {
		WB_LOGI("wifi fw: app=%s geps=%s wlan=%s",
			mi->app_ver[0] ? mi->app_ver : "?",
			mi->geps_ver[0] ? mi->geps_ver : "?",
			mi->wlan_ver[0] ? mi->wlan_ver : "?");
	} else if (mi->version[0]) {
		WB_LOGI("wifi fw: %s", mi->version);
	} else {
		WB_LOGI("wifi fw: (unavailable)");
	}
}

static void prvLogIp(void)
{
	const char *ip = gs_wifi_last_ip();
	char buf[16];

	if (!ip || !ip[0]) {
		if (gs_wifi_get_ip(buf, sizeof(buf)) == GS_MSG_OK) {
			ip = buf;
		}
	}
	WB_LOGI("wifi ip: %s", (ip && ip[0]) ? ip : "(none)");
}

static void prvLogNtpTime(void)
{
	const char *t = gs_wifi_last_time_str();
	WB_LOGI("wifi ntp: %s (unix=%u synced=%d)",
		(t && t[0]) ? t : "(sync failed)",
		(unsigned)gs_wifi_last_unix_time(),
		wb_time_is_synced() ? 1 : 0);
}

static void prvWifiTask(void *pvParameters)
{
	gs_user_config_t cfg;
	gs_user_state_t prev = GS_USER_IDLE;

	(void)pvParameters;

	memset(&cfg, 0, sizeof(cfg));
	/* Prefer flash credential slot (patchable); else compile-time macros. */
	if (wb_wifi_cred_ssid()) {
		cfg.ssid = wb_wifi_cred_ssid();
		cfg.psk = wb_wifi_cred_psk() ? wb_wifi_cred_psk() : "";
	} else {
		cfg.ssid = WB_WIFI_SSID;
		cfg.psk = WB_WIFI_PSK;
	}
	cfg.use_limited_ap_on_fail = false;

	if (gs_platform_freertos_init(&s_gs_plat) != 0) {
		WB_LOGE("GS platform init failed");
		for (;;) {
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
	}

	WB_LOGI("UART0 %u baud, src=%u Hz, INTF_SEL=%u PGM_idle=%u SBR=%u BRFA=%u",
		(unsigned)GS_UART_BAUD_DEFAULT,
		(unsigned)CLOCK_GetFreq(UART0_CLK_SRC),
		(unsigned)GS_INTF_SEL_UART_LEVEL,
		(unsigned)GS_PGM_IDLE_LEVEL,
		(unsigned)((UART0->BDH & UART_BDH_SBR_MASK) << 8 | UART0->BDL),
		(unsigned)(UART0->C4 & UART_C4_BRFA_MASK));
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

	gs_user_init(&s_user, &cfg);
	WB_LOGI("GS1500M bring-up starting (ssid %s)",
		(cfg.ssid && cfg.ssid[0]) ? cfg.ssid : "(none)");

	for (;;) {
		gs_user_state_t st = gs_user_poll(&s_user);
		if (st != prev) {
			WB_LOGI("wifi SM: %s (msg=%d %s)", prvStateName(st),
				(int)s_user.last_msg, prvMsgName(s_user.last_msg));
			if (prev == GS_USER_INIT && st != GS_USER_ERROR) {
				const gs_wifi_init_diag_t *d = gs_wifi_last_init_diag();
				if (d) {
					WB_LOGI("wifi link: baud=%u intf=%s pgm=%s hw_rst=%d saw_boot=%d rx=%u rst=%u",
						(unsigned)d->baud,
						d->intf_sel < 0 ? "float" :
							(d->intf_sel ? "1" : "0"),
						d->pgm_idle < 0 ? "float" :
							(d->pgm_idle ? "1" : "0"),
						d->hw_reset ? 1 : 0,
						d->saw_boot ? 1 : 0,
						(unsigned)d->rx_bytes,
						(unsigned)d->pins.reset);
				}
			}
			if (st == GS_USER_HTTP_TIME) {
				prvLogModuleInfo();
				prvLogIp();
			}
			if (prev == GS_USER_HTTP_TIME && st != GS_USER_ERROR) {
				prvLogNtpTime();
			}
			if (st == GS_USER_ERROR) {
				const char *line = gs_at_last_line();
				const char *partial = gs_at_partial_line();
				const char *accum = gs_at_info_accum();
				const gs_wifi_init_diag_t *d = gs_wifi_last_init_diag();
				WB_LOGE("last AT line: '%s'", line ? line : "");
				WB_LOGE("AT rx_bytes=%u partial='%s'",
					(unsigned)gs_at_rx_byte_count(),
					(partial && partial[0]) ? partial : "");
				WB_LOGE("AT info: '%s'",
					(accum && accum[0]) ? accum : "");
				if (d) {
					WB_LOGE("init try baud=%u intf=%s pgm=%s hw_rst=%d saw_boot=%d rx=%u rst=%u pgm_pin=%u intf_pin=%u hiz=%u",
						(unsigned)d->baud,
						d->intf_sel < 0 ? "float" :
							(d->intf_sel ? "1" : "0"),
						d->pgm_idle < 0 ? "float" :
							(d->pgm_idle ? "1" : "0"),
						d->hw_reset ? 1 : 0,
						d->saw_boot ? 1 : 0,
						(unsigned)d->rx_bytes,
						(unsigned)d->pins.reset,
						(unsigned)d->pins.pgm,
						(unsigned)d->pins.intf_sel,
						(unsigned)d->pins.intf_hiz);
				}
			}
			prev = st;
		}

		if (st == GS_USER_READY || st == GS_USER_ERROR) {
			/* Keep RX alive; blink LED as heartbeat. */
			gs_platform_freertos_rx_poll(50);
			prvLedToggle();
			vTaskDelay(pdMS_TO_TICKS(500));
		} else {
			gs_platform_freertos_rx_poll(10);
			vTaskDelay(pdMS_TO_TICKS(20));
		}
	}
}

static uint32_t prvMillis(void *ctx)
{
	(void)ctx;
	if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
		return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
	}
	return 0U;
}

int main(void)
{
	BOARD_InitHardware();
	prvLedInit();

	wb_time_init(prvMillis, NULL);
	wb_log_init(wb_log_stdio_backend(), WB_LOG_INFO);

#ifdef LOG_BACKEND_RTT
	SEGGER_RTT_Init();
	WB_LOGI("WunderBar WiFi (FreeRTOS RTT)");
#else
	if (xTaskCreate(prvUsbTask, "usb", USBD_STACK_SIZE, NULL,
			configMAX_PRIORITIES - 1, NULL) != pdPASS) {
		for (;;) {
		}
	}
#endif

	if (xTaskCreate(prvWifiTask, "wifi", WIFI_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + 2, NULL) != pdPASS) {
		for (;;) {
		}
	}

	vTaskStartScheduler();
	for (;;) {
	}
}
