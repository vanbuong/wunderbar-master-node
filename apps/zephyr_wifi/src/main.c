/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr GS1500M WiFi via wifi_mgmt offload (gainspan,gs1500m-at).
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/clock.h>

#include <string.h>
#include <errno.h>
#include <time.h>

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

static K_SEM_DEFINE(wifi_done_sem, 0, 1);
static int wifi_result;

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

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
				    uint64_t mgmt_event, struct net_if *iface)
{
	const struct wifi_status *st = (const struct wifi_status *)cb->info;

	ARG_UNUSED(iface);

	if (mgmt_event == NET_EVENT_WIFI_CONNECT_RESULT) {
		wifi_result = st ? st->status : -1;
		k_sem_give(&wifi_done_sem);
	} else if (mgmt_event == NET_EVENT_WIFI_DISCONNECT_RESULT) {
		LOG_INF("wifi disconnect event");
	}
}

static struct net_if *wait_for_wifi_iface(int timeout_ms)
{
	int64_t end = k_uptime_get() + timeout_ms;

	while (k_uptime_get() < end) {
		struct net_if *iface = net_if_get_first_wifi();

		if (iface && net_if_is_carrier_ok(iface)) {
			return iface;
		}
		k_msleep(100);
	}
	return NULL;
}

static int wifi_connect_sta(struct net_if *iface, const char *ssid, const char *psk)
{
	static struct net_mgmt_event_callback wifi_cb;
	static bool wifi_cb_registered;
	struct wifi_connect_req_params cnx = { 0 };
	int ret;

	if (!ssid || !ssid[0]) {
		LOG_ERR("no SSID configured");
		return -EINVAL;
	}

	if (!wifi_cb_registered) {
		net_mgmt_init_event_callback(&wifi_cb, wifi_mgmt_event_handler,
					     NET_EVENT_WIFI_CONNECT_RESULT |
						     NET_EVENT_WIFI_DISCONNECT_RESULT);
		net_mgmt_add_event_callback(&wifi_cb);
		wifi_cb_registered = true;
	}

	cnx.ssid = (const uint8_t *)ssid;
	cnx.ssid_length = strlen(ssid);
	cnx.psk = (const uint8_t *)psk;
	cnx.psk_length = psk ? strlen(psk) : 0U;
	cnx.security = (psk && psk[0]) ? WIFI_SECURITY_TYPE_PSK
				       : WIFI_SECURITY_TYPE_NONE;
	cnx.channel = WIFI_CHANNEL_ANY;
	cnx.band = WIFI_FREQ_BAND_UNKNOWN;
	cnx.mfp = WIFI_MFP_OPTIONAL;

	k_sem_reset(&wifi_done_sem);
	wifi_result = -ETIMEDOUT;

	LOG_INF("wifi connect ssid=%s", ssid);
	ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx, sizeof(cnx));
	if (ret) {
		LOG_ERR("NET_REQUEST_WIFI_CONNECT failed (%d)", ret);
		return ret;
	}

	if (k_sem_take(&wifi_done_sem, K_SECONDS(60)) != 0) {
		LOG_ERR("connect timed out");
		return -ETIMEDOUT;
	}
	if (wifi_result != 0) {
		LOG_ERR("connect result=%d", wifi_result);
		return wifi_result;
	}
	LOG_INF("wifi connected");
	return 0;
}

static void log_iface_status(struct net_if *iface)
{
	struct wifi_iface_status status = { 0 };

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
		     sizeof(status))) {
		LOG_WRN("iface status request failed");
		return;
	}
	LOG_INF("wifi state=%d ssid=%s security=%d", status.state, status.ssid,
		(int)status.security);
}

/* Prove Zephyr-native TCP offload: open NCTCP (optional tiny HTTP HEAD). */
static int smoke_tcp_one(const char *ip, uint16_t port, bool http_head)
{
	int fd;
	struct sockaddr_in addr;
	char req[96];
	char buf[80];
	int n;

	fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		LOG_ERR("socket() failed (%d)", errno);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (zsock_inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		LOG_ERR("bad smoke IP %s", ip);
		zsock_close(fd);
		return -1;
	}

	LOG_INF("TCP smoke connect %s:%u...", ip, port);
	if (zsock_connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("connect(%s:%u) failed errno=%d", ip, port, errno);
		zsock_close(fd);
		return -1;
	}
	LOG_INF("TCP connected to %s:%u", ip, port);

	if (http_head) {
		n = snprintk(req, sizeof(req),
			     "HEAD / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
			     ip);
		if (n > 0 && zsock_send(fd, req, (size_t)n, 0) >= 0) {
			n = zsock_recv(fd, buf, sizeof(buf) - 1U, 0);
			if (n > 0) {
				buf[n] = '\0';
				LOG_INF("TCP rx %d bytes: %.60s", n, buf);
			} else {
				LOG_WRN("TCP recv returned %d errno=%d", n, errno);
			}
		} else {
			LOG_WRN("HTTP HEAD send skipped/failed (%d)", errno);
		}
	}

	zsock_close(fd);
	return 0;
}

static void socket_smoke_tcp(void)
{
	/* Prefer HTTP; fall back to TCP/53 (often allowed when :80 is filtered). */
	if (smoke_tcp_one("1.1.1.1", 80, true) == 0) {
		LOG_INF("TCP smoke OK via 1.1.1.1:80");
		return;
	}
	k_msleep(500);
	if (smoke_tcp_one("8.8.8.8", 53, false) == 0) {
		LOG_INF("TCP smoke OK via 8.8.8.8:53");
		return;
	}
	k_msleep(500);
	if (smoke_tcp_one("208.67.222.222", 80, true) == 0) {
		LOG_INF("TCP smoke OK via 208.67.222.222:80");
		return;
	}
	LOG_ERR("TCP smoke failed on all targets");
}

int main(void)
{
	struct net_if *iface;
	const char *ssid;
	const char *psk;

	wb_time_init(NULL, NULL);

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}
	(void)gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

#if CONSOLE_IS_USB_CDC
	wait_for_dtr();
	LOG_INF("WunderBar WiFi (Zephyr USB, GS1500M offload)");
#else
	LOG_INF("WunderBar WiFi (Zephyr RTT, GS1500M offload)");
#endif

	if (wb_wifi_cred_ssid()) {
		ssid = wb_wifi_cred_ssid();
		psk = wb_wifi_cred_psk() ? wb_wifi_cred_psk() : "";
	} else {
		ssid = CONFIG_WB_WIFI_SSID;
		psk = CONFIG_WB_WIFI_PSK;
	}

	{
		const wb_wifi_cred_t *c = wb_wifi_cred_at_flash();

		LOG_INF("cred @%p (expect 0x%08X) valid=%d ssid=%s",
			(void *)c,
			(unsigned)WB_WIFI_CRED_FLASH_ADDR,
			wb_wifi_cred_valid() ? 1 : 0,
			(ssid && ssid[0]) ? ssid : "(none)");
		if (!wb_wifi_cred_valid() && c) {
			LOG_WRN("cred magic=%02X%02X%02X%02X%02X%02X%02X%02X "
				"(patch .elf+.hex, then reflash; west flash uses .hex)",
				(unsigned)(uint8_t)c->magic[0],
				(unsigned)(uint8_t)c->magic[1],
				(unsigned)(uint8_t)c->magic[2],
				(unsigned)(uint8_t)c->magic[3],
				(unsigned)(uint8_t)c->magic[4],
				(unsigned)(uint8_t)c->magic[5],
				(unsigned)(uint8_t)c->magic[6],
				(unsigned)(uint8_t)c->magic[7]);
		}
	}

	LOG_INF("waiting for GS1500M iface...");
	iface = wait_for_wifi_iface(30000);
	if (!iface) {
		LOG_ERR("WiFi iface not ready");
		return 0;
	}
	LOG_INF("GS1500M iface ready");

	{
		int tries = 0;
		int ret;

		do {
			ret = wifi_connect_sta(iface, ssid, psk);
			if (ret != -EAGAIN) {
				break;
			}
			tries++;
			LOG_WRN("connect not ready yet, retry %d", tries);
			k_msleep(500);
		} while (tries < 10);

		if (ret == 0) {
			log_iface_status(iface);
			LOG_INF("realtime synced=%d unix=%u",
				wb_time_is_synced() ? 1 : 0,
				(unsigned)wb_time_get_unix());
			net_if_set_default(iface);
			socket_smoke_tcp();
		}
	}

	while (1) {
		(void)gpio_pin_toggle_dt(&led);
		k_msleep(500);
	}

	return 0;
}
