/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/wifi.h"
#include "gs1500m/platform.h"
#include "gs1500m/pins.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static gs_wifi_module_info_t s_module_info;
static gs_wifi_init_diag_t s_init_diag;

static void delay_ms(uint32_t ms)
{
	const gs_platform_t *p = gs_platform_get();
	if (p && p->delay_ms) {
		p->delay_ms(ms, p->ctx);
	}
}

static uint32_t now_ms(void)
{
	const gs_platform_t *p = gs_platform_get();
	return (p && p->millis) ? p->millis(p->ctx) : 0U;
}

static void copy_field(char *dst, size_t dst_len, const char *src)
{
	if (!dst || dst_len == 0U) {
		return;
	}
	if (!src) {
		dst[0] = '\0';
		return;
	}
	strncpy(dst, src, dst_len - 1U);
	dst[dst_len - 1U] = '\0';
}

static const char *find_keyed_value(const char *text, const char *key)
{
	const char *p;
	if (!text || !key) {
		return NULL;
	}
	p = strstr(text, key);
	if (!p) {
		return NULL;
	}
	p += strlen(key);
	while (*p == ' ' || *p == '\t' || *p == '=') {
		p++;
	}
	return (*p != '\0') ? p : NULL;
}

static void copy_token(char *dst, size_t dst_len, const char *src)
{
	size_t i = 0;
	if (!dst || dst_len == 0U) {
		return;
	}
	if (!src) {
		dst[0] = '\0';
		return;
	}
	while (src[i] && src[i] != '\r' && src[i] != '\n' && src[i] != ' ' &&
	       i + 1U < dst_len) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

static bool looks_like_mac(const char *s)
{
	size_t n;
	size_t i;
	if (!s) {
		return false;
	}
	n = strlen(s);
	if (n < 17U) {
		return false;
	}
	for (i = 0; i < 17U; i++) {
		if ((i % 3U) == 2U) {
			if (s[i] != ':' && s[i] != '-') {
				return false;
			}
		} else if (!isxdigit((unsigned char)s[i])) {
			return false;
		}
	}
	return true;
}

static void parse_version_blob(gs_wifi_module_info_t *info, const char *blob)
{
	const char *v;

	if (!info) {
		return;
	}
	copy_field(info->version, sizeof(info->version), blob);
	v = find_keyed_value(blob, "S2W APP VERSION");
	if (!v) {
		v = find_keyed_value(blob, "APP VERSION");
	}
	if (v) {
		copy_token(info->app_ver, sizeof(info->app_ver), v);
	}
	v = find_keyed_value(blob, "S2W GEPS VERSION");
	if (!v) {
		v = find_keyed_value(blob, "GEPS VERSION");
	}
	if (v) {
		copy_token(info->geps_ver, sizeof(info->geps_ver), v);
	}
	v = find_keyed_value(blob, "S2W WLAN VERSION");
	if (!v) {
		v = find_keyed_value(blob, "WLAN VERSION");
	}
	if (v) {
		copy_token(info->wlan_ver, sizeof(info->wlan_ver), v);
	}

	if (strstr(blob, "Serial2WiFi") || strstr(blob, "S2W")) {
		copy_field(info->name, sizeof(info->name), "Serial2WiFi");
	} else if (!info->name[0]) {
		copy_field(info->name, sizeof(info->name), "GS1500M");
	}
}

static void parse_mac_blob(gs_wifi_module_info_t *info, const char *blob)
{
	const char *p;
	char tmp[32];

	if (!info || !blob) {
		return;
	}
	p = find_keyed_value(blob, "MAC");
	if (!p) {
		p = blob;
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			p++;
		}
	}
	copy_token(tmp, sizeof(tmp), p);
	if (looks_like_mac(tmp)) {
		copy_field(info->mac, sizeof(info->mac), tmp);
	}
}

gs_msg_id_t gs_wifi_echo(bool on)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "ATE%u\r\n",
			       on ? 1U : 0U);
}

gs_msg_id_t gs_wifi_soft_reset(void)
{
	return gs_at_send_cmd("AT+RESET\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}

gs_msg_id_t gs_wifi_bulk_data(bool on)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+BDATA=%u\r\n",
			       on ? 1U : 0U);
}

gs_msg_id_t gs_wifi_radio(bool on)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+WRXACTIVE=%u\r\n",
			       on ? 1U : 0U);
}

gs_msg_id_t gs_wifi_set_mode(gs_wifi_mode_t mode)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+WM=%u\r\n",
			       (unsigned)mode);
}

gs_msg_id_t gs_wifi_set_security(gs_wifi_security_t sec)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+WSEC=%u\r\n",
			       (unsigned)sec);
}

gs_msg_id_t gs_wifi_set_passphrase(const char *psk)
{
	if (!psk) {
		return GS_MSG_INVALID_INPUT;
	}
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+WWPA=%s\r\n", psk);
}

gs_msg_id_t gs_wifi_dhcp_client(bool on)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+NDHCP=%u\r\n",
			       on ? 1U : 0U);
}

gs_msg_id_t gs_wifi_set_ip(const char *ip, const char *mask, const char *gw)
{
	if (!ip || !mask || !gw) {
		return GS_MSG_INVALID_INPUT;
	}
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+NSET=%s,%s,%s\r\n",
			       ip, mask, gw);
}

gs_msg_id_t gs_wifi_version(char *out, size_t out_len)
{
	gs_msg_id_t id = gs_at_send_cmd("AT+VER=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	if (out && out_len > 0U) {
		const char *blob = gs_at_info_accum();
		if (!blob || !blob[0]) {
			blob = gs_at_last_info_line();
		}
		if (!blob || !blob[0]) {
			blob = gs_at_last_line();
		}
		strncpy(out, blob, out_len - 1U);
		out[out_len - 1U] = '\0';
	}
	return id;
}

gs_msg_id_t gs_wifi_query_module_info(gs_wifi_module_info_t *out)
{
	gs_wifi_module_info_t info;
	gs_msg_id_t id_ver;
	gs_msg_id_t id_mac;
	const char *blob;

	memset(&info, 0, sizeof(info));
	copy_field(info.name, sizeof(info.name), "GS1500M");

	id_ver = gs_at_send_cmd("AT+VER=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if (id_ver == GS_MSG_OK && blob && blob[0]) {
		parse_version_blob(&info, blob);
	}

	id_mac = gs_at_send_cmd("AT+NMAC=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if (id_mac == GS_MSG_OK && blob && blob[0]) {
		parse_mac_blob(&info, blob);
	}

	s_module_info = info;
	if (out) {
		*out = info;
	}

	if (id_ver == GS_MSG_OK || id_mac == GS_MSG_OK) {
		return GS_MSG_OK;
	}
	return (id_ver != GS_MSG_NONE) ? id_ver : id_mac;
}

const gs_wifi_module_info_t *gs_wifi_last_module_info(void)
{
	return &s_module_info;
}

const gs_wifi_init_diag_t *gs_wifi_last_init_diag(void)
{
	return &s_init_diag;
}

static void diag_sample(uint32_t baud, int intf_sel, bool saw_boot)
{
	const gs_platform_t *p = gs_platform_get();

	memset(&s_init_diag, 0, sizeof(s_init_diag));
	s_init_diag.baud = baud;
	s_init_diag.intf_sel = intf_sel;
	s_init_diag.saw_boot = saw_boot;
	s_init_diag.rx_bytes = gs_at_rx_byte_count();
	if (p && p->ctrl_pins_get) {
		p->ctrl_pins_get(&s_init_diag.pins, p->ctx);
	}
}

static bool wait_boot_banner(uint32_t timeout_ms)
{
	const gs_platform_t *p = gs_platform_get();
	uint32_t start = now_ms();

	if (!p || !p->uart_read) {
		return false;
	}
	while ((now_ms() - start) < timeout_ms) {
		uint8_t b;
		if (p->uart_read(&b, 1, 20U, p->ctx) > 0) {
			gs_msg_id_t mid = gs_at_process_byte(b);
			if (mid == GS_MSG_WELCOME || mid == GS_MSG_APP_RESET) {
				return true;
			}
		} else {
			delay_ms(10);
		}
	}
	return false;
}

static gs_msg_id_t probe_at(uint32_t timeout_ms)
{
	gs_msg_id_t id;

	(void)gs_at_write((const uint8_t *)"\r\n", 2U);
	delay_ms(30);
	gs_at_flush();
	id = gs_at_send_cmd("AT\r\n", timeout_ms);
	return id;
}

static gs_msg_id_t try_link_once(uint32_t baud, int intf_sel, uint32_t boot_ms)
{
	const gs_platform_t *p = gs_platform_get();
	bool saw_boot;
	gs_msg_id_t id;

	if (!p) {
		return GS_MSG_ERROR;
	}

	gs_at_init(NULL);

	if (p->uart_set_baud && p->uart_set_baud(baud, p->ctx) != 0) {
		return GS_MSG_ERROR;
	}
	if (p->intf_sel_set) {
		p->intf_sel_set(intf_sel, p->ctx);
	} else if (p->intf_sel_uart && intf_sel == (int)GS_INTF_SEL_UART_LEVEL) {
		p->intf_sel_uart(p->ctx);
	}
	if (p->pgm_set) {
		p->pgm_set(false, p->ctx);
	}
	if (p->reset_set) {
		p->reset_set(true, p->ctx);
		delay_ms(200);
		p->reset_set(false, p->ctx);
	}

	saw_boot = wait_boot_banner(boot_ms);
	if (!saw_boot) {
		delay_ms(200);
	}

	id = probe_at(1500U);
	if (id != GS_MSG_OK) {
		/* Soft-reset only helps if the UART already matches. */
		if (gs_at_rx_byte_count() > 0U) {
			(void)gs_wifi_soft_reset();
			delay_ms(1000);
			gs_at_flush();
			id = probe_at(2000U);
		}
	}

	diag_sample((id == GS_MSG_OK) ? baud : 0U, intf_sel, saw_boot);
	if (id != GS_MSG_OK) {
		s_init_diag.baud = baud; /* remember last tried baud */
		s_init_diag.rx_bytes = gs_at_rx_byte_count();
	}
	return id;
}

static gs_msg_id_t maybe_switch_to_115200(uint32_t current_baud)
{
	gs_msg_id_t id;
	const gs_platform_t *p = gs_platform_get();

	if (current_baud == GS_UART_BAUD_DEFAULT) {
		return GS_MSG_OK;
	}
	/* Module ATB takes effect immediately; then retune host UART. */
	id = gs_at_send_cmd("ATB=115200\r\n", 2000U);
	if (id != GS_MSG_OK) {
		return id;
	}
	delay_ms(50);
	if (p && p->uart_set_baud) {
		(void)p->uart_set_baud(GS_UART_BAUD_DEFAULT, p->ctx);
	}
	gs_at_flush();
	id = probe_at(2000U);
	if (id == GS_MSG_OK) {
		s_init_diag.baud = GS_UART_BAUD_DEFAULT;
	}
	return id;
}

gs_msg_id_t gs_wifi_init(uint32_t ready_timeout_ms)
{
	const gs_platform_t *p = gs_platform_get();
	static const uint32_t bauds[] = { 115200U, 9600U, 57600U };
	static const int intf_modes[] = { -1, 1, 0 }; /* float, high, low */
	uint32_t boot_ms;
	size_t bi;
	size_t ii;
	gs_msg_id_t id = GS_MSG_TIMEOUT;
	uint32_t ok_baud = 0U;

	if (!p) {
		return GS_MSG_ERROR;
	}

	boot_ms = ready_timeout_ms / 4U;
	if (boot_ms < 800U) {
		boot_ms = 800U;
	}
	if (boot_ms > 2500U) {
		boot_ms = 2500U;
	}

	memset(&s_init_diag, 0, sizeof(s_init_diag));
	s_init_diag.intf_sel = -1;

	for (bi = 0; bi < sizeof(bauds) / sizeof(bauds[0]); bi++) {
		for (ii = 0; ii < sizeof(intf_modes) / sizeof(intf_modes[0]); ii++) {
			id = try_link_once(bauds[bi], intf_modes[ii], boot_ms);
			if (id == GS_MSG_OK) {
				ok_baud = bauds[bi];
				goto linked;
			}
		}
	}
	return id;

linked:
	(void)maybe_switch_to_115200(ok_baud);

	id = gs_wifi_echo(false);
	if (id != GS_MSG_OK) {
		return id;
	}

	id = gs_wifi_bulk_data(true);
	if (id != GS_MSG_OK) {
		return id;
	}

	id = gs_wifi_radio(true);
	if (id != GS_MSG_OK) {
		return id;
	}

	(void)gs_wifi_query_module_info(NULL);
	return GS_MSG_OK;
}

gs_msg_id_t gs_wifi_join(const char *ssid, const char *bssid_or_null,
			 const char *channel_or_null)
{
	if (!ssid) {
		return GS_MSG_INVALID_INPUT;
	}
	if (bssid_or_null && channel_or_null) {
		return gs_at_send_cmdf(30000U, "AT+WA=%s,%s,%s\r\n", ssid,
				       bssid_or_null, channel_or_null);
	}
	if (channel_or_null) {
		return gs_at_send_cmdf(30000U, "AT+WA=%s,,%s\r\n", ssid,
				       channel_or_null);
	}
	return gs_at_send_cmdf(30000U, "AT+WA=%s\r\n", ssid);
}

gs_msg_id_t gs_wifi_disconnect(void)
{
	return gs_at_send_cmd("AT+WD\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}

gs_msg_id_t gs_wifi_get_status(gs_wifi_status_t *st)
{
	gs_msg_id_t id;
	const char *line;
	const char *ip;

	if (!st) {
		return GS_MSG_INVALID_INPUT;
	}
	memset(st, 0, sizeof(*st));

	id = gs_at_send_cmd("AT+NSTAT=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	line = gs_at_last_line();

	/* NSTAT prints multiple lines; last_line is the terminating OK line.
	 * Best-effort: scan during wait would need a richer buffer. For now
	 * try to find IP= in last_line and mark associated if OK. */
	st->associated = (id == GS_MSG_OK);
	ip = strstr(line, "IP=");
	if (!ip) {
		ip = strstr(line, "ip=");
	}
	if (ip) {
		ip += 3;
		size_t i = 0;
		while (*ip && *ip != ' ' && *ip != '\r' && *ip != '\n' &&
		       i + 1U < sizeof(st->ip)) {
			st->ip[i++] = *ip++;
		}
		st->ip[i] = '\0';
	}
	return id;
}

gs_msg_id_t gs_wifi_get_ip(char *ip, size_t ip_len)
{
	gs_wifi_status_t st;
	gs_msg_id_t id = gs_wifi_get_status(&st);
	if (ip && ip_len > 0U) {
		strncpy(ip, st.ip, ip_len - 1U);
		ip[ip_len - 1U] = '\0';
	}
	return id;
}

gs_msg_id_t gs_wifi_get_rssi(int16_t *rssi_dbm)
{
	gs_msg_id_t id = gs_at_send_cmd("AT+WRSSI=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	if (rssi_dbm) {
		const char *info = gs_at_last_info_line();
		*rssi_dbm = (int16_t)atoi(info && info[0] ? info : gs_at_last_line());
	}
	return id;
}

gs_msg_id_t gs_wifi_join_wpa(const char *ssid, const char *psk)
{
	gs_msg_id_t id;

	id = gs_wifi_set_mode(GS_WIFI_MODE_STA);
	if (id != GS_MSG_OK) {
		return id;
	}
	id = gs_wifi_set_security(GS_WIFI_SEC_WPA_WPA2_PSK);
	if (id != GS_MSG_OK) {
		return id;
	}
	if (psk && psk[0]) {
		id = gs_wifi_set_passphrase(psk);
		if (id != GS_MSG_OK) {
			return id;
		}
	}
	id = gs_wifi_dhcp_client(true);
	if (id != GS_MSG_OK) {
		return id;
	}
	return gs_wifi_join(ssid, NULL, NULL);
}
