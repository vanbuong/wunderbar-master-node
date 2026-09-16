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

gs_msg_id_t gs_wifi_init(uint32_t ready_timeout_ms)
{
	const gs_platform_t *p = gs_platform_get();
	uint32_t start;
	gs_msg_id_t id;
	bool saw_boot = false;
	uint32_t min_boot_ms = 2000U;

	if (!p) {
		return GS_MSG_ERROR;
	}
	if (ready_timeout_ms < min_boot_ms) {
		ready_timeout_ms = min_boot_ms;
	}

	gs_at_init(NULL);

	/* INTF_SEL / PGM must be stable before releasing reset. */
	if (p->intf_sel_uart) {
		p->intf_sel_uart(p->ctx);
	}
	if (p->pgm_set) {
		p->pgm_set(false, p->ctx); /* idle / deasserted */
	}

	/* Hardware reset pulse (active low). Hold long enough for GS1500M. */
	if (p->reset_set) {
		p->reset_set(true, p->ctx);
		delay_ms(200);
		p->reset_set(false, p->ctx);
	}

	/* Wait for Serial2WiFi banner (always ≥ min_boot_ms after reset). */
	start = now_ms();
	while ((now_ms() - start) < ready_timeout_ms) {
		uint8_t b;
		if (p->uart_read && p->uart_read(&b, 1, 20U, p->ctx) > 0) {
			gs_msg_id_t mid = gs_at_process_byte(b);
			if (mid == GS_MSG_WELCOME || mid == GS_MSG_APP_RESET) {
				saw_boot = true;
				break;
			}
		} else {
			delay_ms(10);
		}
	}

	if (!saw_boot) {
		delay_ms(500);
	}

	/* Nudge + flush stale boot noise before probing. */
	(void)gs_at_write((const uint8_t *)"\r\n", 2U);
	delay_ms(50);
	gs_at_flush();

	/* Probe link before soft-reset. */
	id = gs_at_send_cmd("AT\r\n", 3000U);
	if (id != GS_MSG_OK) {
		id = gs_wifi_soft_reset();
		if (id != GS_MSG_OK && id != GS_MSG_WELCOME && id != GS_MSG_APP_RESET &&
		    id != GS_MSG_TIMEOUT) {
			/* Keep going; many firmwares only print a banner. */
		}
		delay_ms(1500);
		gs_at_flush();
		id = gs_at_send_cmd("AT\r\n", 5000U);
		if (id != GS_MSG_OK) {
			return id;
		}
	}

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

	/* Best-effort identity dump; bring-up still succeeds if VER/MAC fail. */
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
