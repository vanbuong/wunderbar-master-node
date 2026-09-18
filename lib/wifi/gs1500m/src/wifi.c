/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/wifi.h"
#include "gs1500m/platform.h"
#include "gs1500m/pins.h"
#include "gs1500m/socket.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#ifndef GS_WIFI_NTP_HOST
#define GS_WIFI_NTP_HOST "pool.ntp.org"
#endif
#ifndef GS_WIFI_NTP_PORT
#define GS_WIFI_NTP_PORT 123U
#endif
#ifndef GS_WIFI_NTP_LOCAL_PORT
#define GS_WIFI_NTP_LOCAL_PORT 12300U
#endif

/* NTP epoch (1900) → Unix epoch (1970) */
#define GS_NTP_UNIX_DELTA 2208988800UL

static gs_wifi_module_info_t s_module_info;
static gs_wifi_init_diag_t s_init_diag;
static gs_wifi_status_t s_last_status;
static char s_last_time_str[40];
static uint32_t s_last_unix_time;

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

static bool blob_looks_like_ip_dump(const char *blob)
{
	/* WA/NSTAT lines: "192.168.x.x:255.255.255.0:192.168.x.x" */
	return blob && (strstr(blob, "IP addr") || strstr(blob, "SubNet") ||
			(strchr(blob, ':') && strstr(blob, "255.")));
}

static void parse_version_blob(gs_wifi_module_info_t *info, const char *blob)
{
	const char *v;

	if (!info || !blob) {
		return;
	}
	/* Never treat association IP dumps as a firmware version string. */
	if (!blob_looks_like_ip_dump(blob)) {
		copy_field(info->version, sizeof(info->version), blob);
	}
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

static bool looks_like_mac_hex12(const char *s)
{
	size_t i;
	if (!s || strlen(s) < 12U) {
		return false;
	}
	for (i = 0; i < 12U; i++) {
		if (!isxdigit((unsigned char)s[i])) {
			return false;
		}
	}
	return true;
}

static void format_mac_hex12(char *dst, size_t dst_len, const char *hex12)
{
	if (!dst || dst_len < 18U || !hex12) {
		return;
	}
	snprintf(dst, dst_len, "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
		 hex12[0], hex12[1], hex12[2], hex12[3], hex12[4], hex12[5],
		 hex12[6], hex12[7], hex12[8], hex12[9], hex12[10], hex12[11]);
}

static void parse_mac_blob(gs_wifi_module_info_t *info, const char *blob)
{
	const char *p;
	char tmp[32];

	if (!info || !blob) {
		return;
	}
	p = find_keyed_value(blob, "MAC Address");
	if (!p) {
		p = find_keyed_value(blob, "MAC");
	}
	if (!p) {
		p = blob;
		while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
			p++;
		}
	}
	/* Skip non-hex prefix like "Address=" leftovers. */
	while (*p && !isxdigit((unsigned char)*p)) {
		p++;
	}
	copy_token(tmp, sizeof(tmp), p);
	if (looks_like_mac(tmp)) {
		copy_field(info->mac, sizeof(info->mac), tmp);
	} else if (looks_like_mac_hex12(tmp)) {
		format_mac_hex12(info->mac, sizeof(info->mac), tmp);
	}
}

static void extract_ip_from_text(const char *text, char *ip, size_t ip_len)
{
	const char *keys[] = { "IP addr=", "IP Addr=", "IP=", "ip=", NULL };
	const char *ip_src = NULL;
	const char *p;
	size_t k;

	if (!ip || ip_len == 0U) {
		return;
	}
	ip[0] = '\0';
	if (!text) {
		return;
	}
	for (k = 0; keys[k]; k++) {
		ip_src = strstr(text, keys[k]);
		if (ip_src) {
			ip_src += strlen(keys[k]);
			break;
		}
	}
	/* GainSpan WA dumps often use "IP" + spaces + "=". */
	if (!ip_src) {
		p = text;
		while ((p = strstr(p, "IP")) != NULL) {
			const char *q;
			if (p > text && (isalnum((unsigned char)p[-1]) ||
					 p[-1] == '_')) {
				p++;
				continue;
			}
			q = p + 2;
			if (strncmp(q, " addr", 5) == 0 ||
			    strncmp(q, " Addr", 5) == 0) {
				q += 5;
			}
			while (*q == ' ' || *q == '\t') {
				q++;
			}
			if (*q == '=') {
				ip_src = q + 1;
				break;
			}
			p++;
		}
	}
	if (!ip_src) {
		return;
	}
	while (*ip_src == ' ' || *ip_src == '\t') {
		ip_src++;
	}
	copy_token(ip, ip_len, ip_src);
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

	/* GS docs often use AT+VER=?? ; try both. */
	id_ver = gs_at_send_cmd("AT+VER=??\r\n", 5000U);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if ((!blob || strlen(blob) < 3U) && id_ver != GS_MSG_OK) {
		id_ver = gs_at_send_cmd("AT+VER=?\r\n", 5000U);
		blob = gs_at_info_accum();
		if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
			blob = gs_at_last_info_line();
		}
	}
	/* Ignore tiny garbage blobs (e.g. a single noise character). */
	if (blob && strlen(blob) >= 3U) {
		parse_version_blob(&info, blob);
	}

	id_mac = gs_at_send_cmd("AT+NMAC=?\r\n", 5000U);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if (blob && blob[0]) {
		parse_mac_blob(&info, blob);
	}

	s_module_info = info;
	if (out) {
		*out = info;
	}

	if (id_ver == GS_MSG_OK || id_mac == GS_MSG_OK || info.version[0] ||
	    info.app_ver[0] || info.mac[0]) {
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

static void diag_sample(uint32_t baud, int intf_sel, int8_t pgm_idle,
			bool hw_reset, bool saw_boot)
{
	const gs_platform_t *p = gs_platform_get();

	memset(&s_init_diag, 0, sizeof(s_init_diag));
	s_init_diag.baud = baud;
	s_init_diag.intf_sel = intf_sel;
	s_init_diag.pgm_idle = pgm_idle;
	s_init_diag.hw_reset = hw_reset;
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

static void apply_pgm(int8_t pgm_idle)
{
	const gs_platform_t *p = gs_platform_get();

	if (!p) {
		return;
	}
	if (pgm_idle < 0) {
		/* PE: no PTE6 init — float / board pull. */
		if (p->pgm_set) {
			p->pgm_set(false, p->ctx);
		}
		return;
	}
	if (p->pgm_level_set) {
		p->pgm_level_set((uint8_t)pgm_idle, p->ctx);
	} else if (p->pgm_set) {
		p->pgm_set(pgm_idle != 0, p->ctx);
	}
}

/**
 * Hardware reset into a clean run state: PGM = float (not program mode),
 * EXT_RESETn pulsed low then released to input/pull-up.
 */
void gs_wifi_hw_reset(void)
{
	const gs_platform_t *p = gs_platform_get();

	if (!p || !p->reset_set) {
		return;
	}

	/* Run mode: PGM must not be high across reset (that enters flash download). */
	apply_pgm(-1);
	if (p->intf_sel_set) {
		p->intf_sel_set(-1, p->ctx); /* float — board pull selects UART */
	}
	delay_ms(10);

	gs_at_flush();
	p->reset_set(true, p->ctx);  /* drive EXT_RESETn low */
	delay_ms(100);
	p->reset_set(false, p->ctx); /* release to input / pull-up */
	/*
	 * Brief settle only — platform delay drains RX into the ring so the
	 * boot banner is not lost to HW FIFO overrun before wait_boot_banner.
	 */
	delay_ms(20);
}

static gs_msg_id_t try_link_once(uint32_t baud, int intf_sel, int8_t pgm_idle,
				 bool hw_reset, uint32_t boot_ms)
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

	apply_pgm(pgm_idle);
	delay_ms(20);

	if (hw_reset && p->reset_set) {
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
		if (gs_at_rx_byte_count() > 0U) {
			(void)gs_wifi_soft_reset();
			delay_ms(1000);
			gs_at_flush();
			id = probe_at(2000U);
		}
	}

	diag_sample((id == GS_MSG_OK) ? baud : 0U, intf_sel, pgm_idle, hw_reset,
		     saw_boot);
	if (id != GS_MSG_OK) {
		s_init_diag.baud = baud;
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
	/*
	 * Always hard-reset first so the module leaves any hung AT/association
	 * state. Then probe with PGM float and optional extra reset strategies.
	 */
	static const bool hw_resets[] = { false, true };
	static const int8_t pgm_modes[] = { -1, 0 }; /* float (PE), then drive low */
	static const uint32_t bauds[] = { 115200U, 9600U };
	static const int intf_modes[] = { -1, 1, 0 };
	uint32_t boot_ms;
	size_t ri;
	size_t pi;
	size_t bi;
	size_t ii;
	gs_msg_id_t id = GS_MSG_TIMEOUT;
	uint32_t ok_baud = 0U;
	bool saw_boot;

	if (!p) {
		return GS_MSG_ERROR;
	}

	boot_ms = ready_timeout_ms / 4U;
	if (boot_ms < 600U) {
		boot_ms = 600U;
	}
	if (boot_ms > 1500U) {
		boot_ms = 1500U;
	}

	memset(&s_init_diag, 0, sizeof(s_init_diag));
	s_init_diag.intf_sel = -1;
	s_init_diag.pgm_idle = -1;

	gs_at_init(NULL);
	if (p->uart_set_baud) {
		(void)p->uart_set_baud(GS_UART_BAUD_DEFAULT, p->ctx);
	}

	/* Fresh module state before any AT traffic. */
	gs_wifi_hw_reset();
	saw_boot = wait_boot_banner(boot_ms);
	if (!saw_boot) {
		delay_ms(300);
	}
	gs_at_flush();
	s_init_diag.hw_reset = true;
	s_init_diag.saw_boot = saw_boot;

	for (ri = 0; ri < sizeof(hw_resets) / sizeof(hw_resets[0]); ri++) {
		for (pi = 0; pi < sizeof(pgm_modes) / sizeof(pgm_modes[0]); pi++) {
			for (bi = 0; bi < sizeof(bauds) / sizeof(bauds[0]); bi++) {
				for (ii = 0; ii < sizeof(intf_modes) / sizeof(intf_modes[0]);
				     ii++) {
					id = try_link_once(bauds[bi], intf_modes[ii],
							   pgm_modes[pi], hw_resets[ri],
							   boot_ms);
					if (id == GS_MSG_OK) {
						ok_baud = bauds[bi];
						/* Preserve that we hard-reset at start. */
						s_init_diag.hw_reset = true;
						if (saw_boot) {
							s_init_diag.saw_boot = true;
						}
						goto linked;
					}
				}
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

	id = gs_wifi_radio(true);
	if (id != GS_MSG_OK) {
		return id;
	}

	/*
	 * Leave BDATA off until after association. Some GS1500M builds are
	 * unstable associating with bulk mode already enabled.
	 */
	gs_at_flush();
	delay_ms(200);
	return GS_MSG_OK;
}

static void harvest_ip_from_last_info(void)
{
	const char *blob = gs_at_info_accum();

	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	extract_ip_from_text(blob, s_last_status.ip, sizeof(s_last_status.ip));
	if (!s_last_status.ip[0]) {
		extract_ip_from_text(gs_at_partial_line(), s_last_status.ip,
				     sizeof(s_last_status.ip));
	}
	if (s_last_status.ip[0]) {
		s_last_status.associated = true;
	}
}

static void drain_rx_ms(uint32_t ms)
{
	const gs_platform_t *p = gs_platform_get();
	uint32_t start = now_ms();

	if (!p || !p->uart_read) {
		return;
	}
	while ((now_ms() - start) < ms) {
		uint8_t b;
		int n = p->uart_read(&b, 1, 20U, p->ctx);
		if (n > 0) {
			(void)gs_at_process_byte(b);
		} else {
			delay_ms(1);
		}
	}
}

gs_msg_id_t gs_wifi_join(const char *ssid, const char *bssid_or_null,
			 const char *channel_or_null)
{
	gs_msg_id_t id;

	if (!ssid) {
		return GS_MSG_INVALID_INPUT;
	}
	/* Association dump can be slow; 45s matches busy DHCP APs. */
	if (bssid_or_null && channel_or_null) {
		id = gs_at_send_cmdf(45000U, "AT+WA=%s,%s,%s\r\n", ssid,
				     bssid_or_null, channel_or_null);
	} else if (channel_or_null) {
		id = gs_at_send_cmdf(45000U, "AT+WA=%s,,%s\r\n", ssid,
				     channel_or_null);
	} else {
		id = gs_at_send_cmdf(45000U, "AT+WA=%s\r\n", ssid);
	}

	if (id == GS_MSG_TIMEOUT) {
		/* Finish a mid-line IP dump if the final OK was late. */
		drain_rx_ms(1000U);
	}

	/* AT+WA prints IP addr=… before OK — capture even on timeout. */
	harvest_ip_from_last_info();
	if (id == GS_MSG_APP_RESET) {
		return id;
	}
	if (id == GS_MSG_TIMEOUT && s_last_status.ip[0]) {
		return GS_MSG_OK;
	}
	if (id == GS_MSG_TIMEOUT) {
		/* Also accept association keywords without a parsed IP yet. */
		const char *blob = gs_at_info_accum();
		const char *partial = gs_at_partial_line();
		if ((blob && (strstr(blob, "ASSOCIATED") || strstr(blob, "SSID") ||
			      strstr(blob, "IP"))) ||
		    (partial && strstr(partial, "IP"))) {
			s_last_status.associated = true;
			return GS_MSG_OK;
		}
		/* Warm-boot banner often arrives fragmented. */
		if ((partial && strstr(partial, "UnEx")) ||
		    (blob && strstr(blob, "UnExpected"))) {
			return GS_MSG_APP_RESET;
		}
	}
	if (id == GS_MSG_OK) {
		s_last_status.associated = true;
	}
	return id;
}

gs_msg_id_t gs_wifi_disconnect(void)
{
	return gs_at_send_cmd("AT+WD\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}

gs_msg_id_t gs_wifi_get_status(gs_wifi_status_t *st)
{
	gs_msg_id_t id;
	const char *blob;
	gs_wifi_status_t local;

	memset(&local, 0, sizeof(local));

	id = gs_at_send_cmd("AT+NSTAT=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if ((!blob || !blob[0])) {
		blob = gs_at_last_line();
	}

	local.associated = (id == GS_MSG_OK);
	extract_ip_from_text(blob, local.ip, sizeof(local.ip));

	{
		const char *v = find_keyed_value(blob, "SubNet");
		if (!v) {
			v = find_keyed_value(blob, "Subnet");
		}
		if (v) {
			copy_token(local.subnet, sizeof(local.subnet), v);
		}
		v = find_keyed_value(blob, "Gateway");
		if (v) {
			copy_token(local.gateway, sizeof(local.gateway), v);
		}
		v = find_keyed_value(blob, "DNS");
		if (v) {
			copy_token(local.dns, sizeof(local.dns), v);
		}
		v = find_keyed_value(blob, "SSID");
		if (v) {
			copy_token(local.ssid, sizeof(local.ssid), v);
		}
		v = find_keyed_value(blob, "MAC");
		if (v) {
			copy_token(local.mac, sizeof(local.mac), v);
		}
	}

	s_last_status = local;
	if (st) {
		*st = local;
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

const char *gs_wifi_last_ip(void)
{
	return s_last_status.ip;
}

const char *gs_wifi_last_time_str(void)
{
	return s_last_time_str;
}

uint32_t gs_wifi_last_unix_time(void)
{
	return s_last_unix_time;
}

static void unix_to_ymdhms(uint32_t unix_sec, int *y, int *mo, int *d,
			   int *hh, int *mm, int *ss)
{
	/* Civil from days (Howard Hinnant) — UTC, no leap seconds. */
	int64_t z;
	int64_t era;
	unsigned doe;
	unsigned yoe;
	unsigned doy;
	unsigned mp;
	int year;
	int month;
	int day;

	*ss = (int)(unix_sec % 60U);
	unix_sec /= 60U;
	*mm = (int)(unix_sec % 60U);
	unix_sec /= 60U;
	*hh = (int)(unix_sec % 24U);
	unix_sec /= 24U;

	z = (int64_t)unix_sec + 719468LL;
	era = (z >= 0) ? (z / 146097LL) : ((z - 146096LL) / 146097LL);
	doe = (unsigned)(z - era * 146097LL);
	yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
	year = (int)(yoe) + (int)(era * 400LL);
	doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
	mp = (5U * doy + 2U) / 153U;
	day = (int)(doy - (153U * mp + 2U) / 5U) + 1;
	month = (int)(mp < 10U ? (mp + 3U) : (mp - 9U));
	year += (month <= 2) ? 1 : 0;

	*y = year;
	*mo = month;
	*d = day;
}

static uint32_t ymdhms_to_unix(int y, int mo, int d, int hh, int mm, int ss)
{
	/* days_from_civil (Howard Hinnant) → Unix seconds UTC. */
	int64_t y0;
	unsigned m0;
	int64_t era;
	unsigned yoe;
	unsigned doy;
	unsigned doe;
	int64_t days;

	y0 = (int64_t)y - (mo <= 2 ? 1 : 0);
	m0 = (unsigned)(mo <= 2 ? (mo + 9) : (mo - 3));
	era = (y0 >= 0) ? (y0 / 400) : ((y0 - 399) / 400);
	yoe = (unsigned)(y0 - era * 400);
	doy = (153U * m0 + 2U) / 5U + (unsigned)(d - 1);
	doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
	days = era * 146097LL + (int64_t)doe - 719468LL;
	if (days < 0 || hh < 0 || mm < 0 || ss < 0 || hh > 23 || mm > 59 ||
	    ss > 60) {
		return 0U;
	}
	return (uint32_t)(days * 86400LL + (int64_t)hh * 3600LL +
			  (int64_t)mm * 60LL + (int64_t)ss);
}

static void format_time_str(char *dst, size_t dst_len, uint32_t unix_sec)
{
	int y, mo, d, hh, mm, ss;

	if (!dst || dst_len == 0U) {
		return;
	}
	unix_to_ymdhms(unix_sec, &y, &mo, &d, &hh, &mm, &ss);
	snprintf(dst, dst_len, "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d, hh, mm,
		 ss);
}

/* Reject GETTIME day-number false positives (e.g. "16" from dd/mm/yyyy). */
static bool unix_time_plausible(uint32_t unix_sec)
{
	/* 2020-01-01 .. ~2100 */
	return unix_sec >= 1577836800UL && unix_sec < 4102444800UL;
}

/*
 * GS docs: AT+GETTIME=? → =<dd/mm/yyyy>,<HH:MM:SS>[,ms since epoch]
 * Also accept a bare millisecond / second epoch field.
 */
static bool parse_gettime_blob(const char *blob, uint32_t *unix_out)
{
	const char *p;
	int dd = 0;
	int mo = 0;
	int yyyy = 0;
	int hh = 0;
	int mm = 0;
	int ss = 0;
	uint64_t ms = 0ULL;
	uint32_t from_civil;
	int n;

	if (!blob || !unix_out) {
		return false;
	}
	p = blob;
	while (*p == ' ' || *p == '\t' || *p == '=' || *p == '\r' || *p == '\n') {
		p++;
	}

	n = sscanf(p, "%d/%d/%d,%d:%d:%d", &dd, &mo, &yyyy, &hh, &mm, &ss);
	if (n == 6 && dd >= 1 && dd <= 31 && mo >= 1 && mo <= 12 && yyyy >= 1970) {
		const char *q = strchr(p, ',');

		from_civil = ymdhms_to_unix(yyyy, mo, dd, hh, mm, ss);
		/* Optional third field: milliseconds since epoch. */
		if (q) {
			q = strchr(q + 1, ',');
			if (q) {
				q++;
				while (*q == ' ' || *q == '\t') {
					q++;
				}
				if (isdigit((unsigned char)*q)) {
					ms = 0ULL;
					while (isdigit((unsigned char)*q)) {
						ms = (ms * 10ULL) +
						     (uint64_t)(*q - '0');
						q++;
					}
					if (ms > 100000000000ULL) {
						*unix_out = (uint32_t)(ms / 1000ULL);
						return unix_time_plausible(*unix_out);
					}
					if (ms > 1000000000ULL) {
						*unix_out = (uint32_t)ms;
						return unix_time_plausible(*unix_out);
					}
				}
			}
		}
		if (unix_time_plausible(from_civil)) {
			*unix_out = from_civil;
			return true;
		}
		return false;
	}

	/* Bare epoch (ms or s) — skip non-digits; require plausible magnitude. */
	while (*p && !isdigit((unsigned char)*p)) {
		p++;
	}
	if (!*p) {
		return false;
	}
	ms = 0ULL;
	while (isdigit((unsigned char)*p)) {
		ms = (ms * 10ULL) + (uint64_t)(*p - '0');
		p++;
	}
	if (ms > 100000000000ULL) {
		*unix_out = (uint32_t)(ms / 1000ULL);
	} else {
		*unix_out = (uint32_t)ms;
	}
	return unix_time_plausible(*unix_out);
}

gs_msg_id_t gs_wifi_settime(uint32_t unix_sec)
{
	int y, mo, d, hh, mm, ss;
	char date[16];
	char time_part[16];

	unix_to_ymdhms(unix_sec, &y, &mo, &d, &hh, &mm, &ss);
	snprintf(date, sizeof(date), "%02d/%02d/%04d", d, mo, y);
	snprintf(time_part, sizeof(time_part), "%02d:%02d:%02d", hh, mm, ss);
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+SETTIME=%s,%s\r\n",
			       date, time_part);
}

gs_msg_id_t gs_wifi_gettime(uint32_t *unix_sec)
{
	gs_msg_id_t id;
	const char *blob;
	uint32_t parsed = 0U;

	id = gs_at_send_cmd("AT+GETTIME=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if ((!blob || !blob[0])) {
		blob = gs_at_last_line();
	}
	/* Also accept unterminated GETTIME body left in the partial line. */
	if ((!blob || !blob[0] || !parse_gettime_blob(blob, &parsed)) &&
	    gs_at_partial_line()[0]) {
		blob = gs_at_partial_line();
	}

	if (blob && parse_gettime_blob(blob, &parsed)) {
		s_last_unix_time = parsed;
		format_time_str(s_last_time_str, sizeof(s_last_time_str), parsed);
		if (unix_sec) {
			*unix_sec = parsed;
		}
	} else if (unix_sec) {
		*unix_sec = 0U;
	}
	return id;
}

typedef struct {
	uint8_t buf[64];
	size_t len;
	bool done;
} gs_ntp_rx_t;

static void ntp_on_data(uint8_t cid, gs_esc_kind_t kind, const uint8_t *data,
			size_t len, void *user)
{
	gs_ntp_rx_t *rx = (gs_ntp_rx_t *)user;

	(void)cid;
	(void)kind;
	if (!rx || !data) {
		return;
	}
	while (len > 0U && rx->len < sizeof(rx->buf)) {
		rx->buf[rx->len++] = *data++;
		len--;
	}
	if (rx->len >= 48U) {
		rx->done = true;
	}
}

static gs_msg_id_t ntp_wait_reply(gs_ntp_rx_t *rx, uint32_t timeout_ms)
{
	const gs_platform_t *p = gs_platform_get();
	uint32_t start = now_ms();

	if (!p || !p->uart_read || !rx) {
		return GS_MSG_ERROR;
	}
	while (!rx->done) {
		uint8_t b;
		int n = p->uart_read(&b, 1, 20U, p->ctx);
		if (n > 0) {
			(void)gs_at_process_byte(b);
		}
		if ((now_ms() - start) >= timeout_ms) {
			return GS_MSG_TIMEOUT;
		}
		if (n <= 0) {
			delay_ms(1);
		}
	}
	return GS_MSG_OK;
}

static uint32_t ntp_extract_unix(const uint8_t pkt[48])
{
	uint32_t secs;

	secs = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) |
	       ((uint32_t)pkt[42] << 8) | (uint32_t)pkt[43];
	if (secs < GS_NTP_UNIX_DELTA) {
		return 0U;
	}
	return secs - GS_NTP_UNIX_DELTA;
}

static bool looks_like_ipv4(const char *s)
{
	int a, b, c, d;

	if (!s) {
		return false;
	}
	return sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 && a >= 0 &&
	       a <= 255 && b >= 0 && b <= 255 && c >= 0 && c <= 255 && d >= 0 &&
	       d <= 255;
}

static gs_msg_id_t dns_lookup_ipv4(const char *host, char *ip, size_t ip_len)
{
	return gs_wifi_dns_lookup(host, ip, ip_len);
}

gs_msg_id_t gs_wifi_dns_lookup(const char *host, char *ip, size_t ip_len)
{
	gs_msg_id_t id;
	const char *blob;
	const char *p;

	if (!host || !host[0] || !ip || ip_len < 8U) {
		return GS_MSG_INVALID_INPUT;
	}
	ip[0] = '\0';
	id = gs_at_send_cmdf(8000U, "AT+DNSLOOKUP=%s,2,3\r\n", host);
	blob = gs_at_info_accum();
	if ((!blob || !blob[0]) && gs_at_last_info_line()[0]) {
		blob = gs_at_last_info_line();
	}
	if ((!blob || !blob[0])) {
		blob = gs_at_last_line();
	}
	if (!blob) {
		return (id == GS_MSG_OK) ? GS_MSG_ERROR : id;
	}
	p = strstr(blob, "IP:");
	if (p) {
		p += 3;
	} else {
		p = blob;
		while (*p && !isdigit((unsigned char)*p)) {
			p++;
		}
	}
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	copy_token(ip, ip_len, p);
	if (!looks_like_ipv4(ip)) {
		ip[0] = '\0';
		return (id == GS_MSG_OK) ? GS_MSG_ERROR : id;
	}
	return GS_MSG_OK;
}

/*
 * Prefer module AT+NTIMESYNC (SNTP on-chip). Server IP is mandatory per docs.
 * One-shot: Enable=1, Periodic=0. Then verify with GETTIME.
 */
static gs_msg_id_t ntp_sync_via_ntimesync(uint32_t *unix_out)
{
	static const char *const servers[] = {
		"162.159.200.1",  /* Cloudflare */
		"216.239.35.0",   /* time.google.com */
		"pool.ntp.org",   /* may need DNSLOOKUP first */
	};
	size_t si;
	char ip[48];
	gs_msg_id_t id;
	uint32_t got = 0U;

	for (si = 0; si < sizeof(servers) / sizeof(servers[0]); si++) {
		ip[0] = '\0';
		if (looks_like_ipv4(servers[si])) {
			copy_field(ip, sizeof(ip), servers[si]);
		} else if (dns_lookup_ipv4(servers[si], ip, sizeof(ip)) !=
			   GS_MSG_OK) {
			continue;
		}

		/* AT+NTIMESYNC=<Enable>,<ServerIP>,<Timeout>,<Periodic> */
		id = gs_at_send_cmdf(20000U, "AT+NTIMESYNC=1,%s,10,0\r\n", ip);
		if (id != GS_MSG_OK) {
			continue;
		}
		delay_ms(200);
		if (gs_wifi_gettime(&got) == GS_MSG_OK &&
		    unix_time_plausible(got)) {
			if (unix_out) {
				*unix_out = got;
			}
			return GS_MSG_OK;
		}
	}
	return GS_MSG_TIMEOUT;
}

static gs_msg_id_t ntp_sync_via_udp(uint32_t *unix_out)
{
	gs_at_callbacks_t cbs;
	gs_at_callbacks_t saved;
	gs_ntp_rx_t rx;
	uint8_t cid = GS_AT_INVALID_CID;
	uint8_t req[48];
	gs_msg_id_t id;
	uint32_t unix_t = 0U;
	static const char *const hosts[] = {
		GS_WIFI_NTP_HOST,
		"time.google.com",
		"162.159.200.1",
	};
	size_t hi;

	memset(&rx, 0, sizeof(rx));
	memset(req, 0, sizeof(req));
	req[0] = 0x1BU; /* LI=0 VN=3 Mode=3 (client) */

	cbs.on_data = ntp_on_data;
	cbs.on_data_end = NULL;
	cbs.on_line = NULL;
	cbs.user = &rx;
	gs_at_get_callbacks(&saved);
	gs_at_set_callbacks(&cbs);

	id = GS_MSG_ERROR;
	for (hi = 0; hi < sizeof(hosts) / sizeof(hosts[0]); hi++) {
		rx.len = 0;
		rx.done = false;
		id = gs_socket_udp_client(hosts[hi], GS_WIFI_NTP_PORT,
					 GS_WIFI_NTP_LOCAL_PORT, &cid);
		if (id != GS_MSG_CONNECT && id != GS_MSG_OK) {
			continue;
		}
		if (cid == GS_AT_INVALID_CID) {
			cid = 0;
		}

		id = gs_socket_send(cid, req, sizeof(req));
		if (id != GS_MSG_OK) {
			(void)gs_socket_close(cid);
			continue;
		}

		id = ntp_wait_reply(&rx, 5000U);
		(void)gs_socket_close(cid);
		if (id == GS_MSG_OK && rx.len >= 48U) {
			unix_t = ntp_extract_unix(rx.buf);
			if (unix_time_plausible(unix_t)) {
				break;
			}
			unix_t = 0U;
		}
		id = GS_MSG_TIMEOUT;
	}

	gs_at_set_callbacks(&saved);

	if (!unix_time_plausible(unix_t)) {
		return (id == GS_MSG_OK) ? GS_MSG_ERROR : id;
	}
	(void)gs_wifi_settime(unix_t);
	if (unix_out) {
		*unix_out = unix_t;
	}
	return GS_MSG_OK;
}

gs_msg_id_t gs_wifi_ntp_sync(uint32_t *unix_sec, char *time_str, size_t time_str_len)
{
	uint32_t unix_t = 0U;
	gs_msg_id_t id;

	s_last_unix_time = 0U;
	s_last_time_str[0] = '\0';

	id = ntp_sync_via_ntimesync(&unix_t);
	if (id != GS_MSG_OK || !unix_time_plausible(unix_t)) {
		id = ntp_sync_via_udp(&unix_t);
	}
	if (id != GS_MSG_OK || !unix_time_plausible(unix_t)) {
		if (time_str && time_str_len > 0U) {
			time_str[0] = '\0';
		}
		if (unix_sec) {
			*unix_sec = 0U;
		}
		return (id == GS_MSG_OK) ? GS_MSG_ERROR : id;
	}

	s_last_unix_time = unix_t;
	format_time_str(s_last_time_str, sizeof(s_last_time_str), unix_t);
	/* Refresh from module when possible; keep prior value on parse miss. */
	{
		uint32_t verified = 0U;

		if (gs_wifi_gettime(&verified) == GS_MSG_OK &&
		    unix_time_plausible(verified)) {
			s_last_unix_time = verified;
			format_time_str(s_last_time_str, sizeof(s_last_time_str),
					verified);
		}
	}

	if (unix_sec) {
		*unix_sec = s_last_unix_time;
	}
	if (time_str && time_str_len > 0U) {
		copy_field(time_str, time_str_len, s_last_time_str);
	}
	return GS_MSG_OK;
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

	if (!ssid) {
		return GS_MSG_INVALID_INPUT;
	}

	/* Drop any prior association; ignore errors. */
	gs_at_flush();
	(void)gs_wifi_disconnect();
	delay_ms(100);

	id = gs_wifi_set_mode(GS_WIFI_MODE_STA);
	if (id != GS_MSG_OK) {
		return id;
	}

	if (psk && psk[0]) {
		/*
		 * Prefer AT+WPAPSK=<ssid>,<passphrase> (computes PSK for this SSID).
		 * PMK derivation on GS1500M is slow — allow up to 30s before falling
		 * back to WSEC + WWPA on older firmwares.
		 */
		id = gs_at_send_cmdf(30000U, "AT+WPAPSK=%s,%s\r\n", ssid, psk);
		if (id == GS_MSG_TIMEOUT) {
			/* Late OK after compute; do not stomp a busy module. */
			drain_rx_ms(2000U);
			{
				const char *partial = gs_at_partial_line();
				const char *last = gs_at_last_line();

				if ((last && gs_at_classify_line(last) == GS_MSG_OK) ||
				    (partial && gs_at_classify_line(partial) == GS_MSG_OK)) {
					id = GS_MSG_OK;
				}
			}
		}
		if (id != GS_MSG_OK) {
			gs_at_flush();
			delay_ms(200);
			id = gs_wifi_set_security(GS_WIFI_SEC_WPA_WPA2_PSK);
			if (id != GS_MSG_OK) {
				return id;
			}
			id = gs_wifi_set_passphrase(psk);
			if (id != GS_MSG_OK) {
				return id;
			}
		}
	} else {
		id = gs_wifi_set_security(GS_WIFI_SEC_OPEN);
		if (id != GS_MSG_OK) {
			return id;
		}
	}

	id = gs_wifi_dhcp_client(true);
	if (id != GS_MSG_OK) {
		return id;
	}

	delay_ms(100);
	id = gs_wifi_join(ssid, NULL, NULL);
	if (id == GS_MSG_APP_RESET) {
		return id;
	}
	return id;
}
