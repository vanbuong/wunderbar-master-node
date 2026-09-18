/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs_stub_platform.h"
#include "gs1500m/at.h"

#include <string.h>

typedef struct {
	char line[GS_AT_TX_CMD_MAX];
	size_t line_len;
	bool in_esc;
	bool was_reset_asserted;
	bool esc_z;
	char esc_kind;
	char esc_cid;
	char len_digits[4];
	size_t len_idx;
	uint32_t bulk_remain;
	bool ntp_udp_open;
} gs_stub_priv_t;

/* Stored at end of user ctx via overlay — keep simple statics reset on install. */
static gs_stub_priv_t s_priv;

static void stub_queue_ntp_reply(gs_stub_ctx_t *ctx)
{
	/* ESC Y cid + IP + ' ' + port + '\t' + 0048 + 48-byte SNTP reply */
	static const uint8_t hdr[] = {
		0x1b, 'Y', '1',
		'1', '6', '2', '.', '1', '5', '9', '.', '2', '0', '0', '.', '1', ' ',
		'1', '2', '3', '\t',
		'0', '0', '4', '8'
	};
	uint8_t pkt[48];
	/* Unix 1758038400 (2025-09-16 16:00:00 UTC) + NTP delta */
	uint32_t ntp_secs = 1758038400UL + 2208988800UL;

	memset(pkt, 0, sizeof(pkt));
	pkt[0] = 0x1C; /* server reply */
	pkt[40] = (uint8_t)((ntp_secs >> 24) & 0xFFU);
	pkt[41] = (uint8_t)((ntp_secs >> 16) & 0xFFU);
	pkt[42] = (uint8_t)((ntp_secs >> 8) & 0xFFU);
	pkt[43] = (uint8_t)(ntp_secs & 0xFFU);

	gs_stub_rx_push(ctx, hdr, sizeof(hdr));
	gs_stub_rx_push(ctx, pkt, sizeof(pkt));
}

static void stub_queue_response_for_cmd(gs_stub_ctx_t *ctx, const char *cmd)
{
	if (!ctx->auto_ok || !cmd) {
		return;
	}

	if (strncmp(cmd, "AT+NCTCP=", 9) == 0 ||
	    strncmp(cmd, "AT+NCUDP=", 9) == 0 ||
	    strncmp(cmd, "AT+NSTCP=", 9) == 0 ||
	    strncmp(cmd, "AT+NSUDP=", 9) == 0 ||
	    strncmp(cmd, "AT+HTTPOPEN=", 12) == 0) {
		if (strncmp(cmd, "AT+NCUDP=", 9) == 0 && strstr(cmd, ",123,")) {
			s_priv.ntp_udp_open = true;
		}
		gs_stub_rx_push_str(ctx, "CONNECT 1\r\n");
		return;
	}
	if (strncmp(cmd, "AT+RESET", 8) == 0) {
		gs_stub_rx_push_str(ctx, "Serial2WiFi APP\r\nOK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+VER=", 7) == 0) {
		gs_stub_rx_push_str(ctx,
				     "Serial2WiFi APP\r\n"
				     "S2W APP VERSION=2.5.1\r\n"
				     "S2W GEPS VERSION=2.5.1\r\n"
				     "S2W WLAN VERSION=2.5.0\r\n"
				     "OK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+NMAC=?", 9) == 0) {
		gs_stub_rx_push_str(ctx, "00:1d:c9:12:34:56\r\nOK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+NSTAT=?", 10) == 0) {
		gs_stub_rx_push_str(ctx,
				     "STATUS:ASSOCIATED\r\n"
				     "SSID=CafeWifi\r\n"
				     "IP addr=192.168.1.50 SubNet=255.255.255.0 "
				     "Gateway=192.168.1.1\r\n"
				     "OK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+GETTIME=?", 12) == 0) {
		/* Doc format: =<dd/mm/yyyy>,<HH:MM:SS>,ms-since-epoch */
		gs_stub_rx_push_str(ctx, "=16/09/2025,16:00:00,1758038400000\r\nOK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+SETTIME=", 11) == 0) {
		gs_stub_rx_push_str(ctx, "OK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+NTIMESYNC=", 13) == 0) {
		gs_stub_rx_push_str(ctx, "OK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+DNSLOOKUP=", 13) == 0) {
		gs_stub_rx_push_str(ctx, "IP:162.159.200.1\r\nOK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+WRSSI=?", 10) == 0) {
		gs_stub_rx_push_str(ctx, "-45\r\nOK\r\n");
		return;
	}
	if (strncmp(cmd, "AT+NCLOSE", 9) == 0) {
		s_priv.ntp_udp_open = false;
		gs_stub_rx_push_str(ctx, "OK\r\n");
		return;
	}
	gs_stub_rx_push_str(ctx, "OK\r\n");
}

static void stub_on_tx_byte(gs_stub_ctx_t *ctx, uint8_t b)
{
	if (s_priv.esc_z) {
		if (s_priv.len_idx == 0U) {
			/* CID digit */
			s_priv.esc_cid = (char)b;
			s_priv.len_idx = 1U;
			return;
		}
		if (s_priv.len_idx < 5U) {
			s_priv.len_digits[s_priv.len_idx - 1U] = (char)b;
			s_priv.len_idx++;
			if (s_priv.len_idx == 5U) {
				s_priv.bulk_remain =
					(uint32_t)(s_priv.len_digits[0] - '0') * 1000U +
					(uint32_t)(s_priv.len_digits[1] - '0') * 100U +
					(uint32_t)(s_priv.len_digits[2] - '0') * 10U +
					(uint32_t)(s_priv.len_digits[3] - '0');
				if (s_priv.bulk_remain == 0U) {
					s_priv.esc_z = false;
					s_priv.len_idx = 0U;
					gs_stub_rx_push_str(ctx, "\x1bO");
				}
			}
			return;
		}
		if (s_priv.bulk_remain > 0U) {
			s_priv.bulk_remain--;
			if (s_priv.bulk_remain == 0U) {
				s_priv.esc_z = false;
				s_priv.len_idx = 0U;
				gs_stub_rx_push_str(ctx, "\x1bO");
				if (s_priv.ntp_udp_open) {
					stub_queue_ntp_reply(ctx);
				}
			}
			return;
		}
		s_priv.esc_z = false;
		return;
	}

	if (b == GS_AT_ESC) {
		s_priv.in_esc = true;
		return;
	}
	if (s_priv.in_esc) {
		char kind = (char)b;
		s_priv.in_esc = false;
		if (kind == 'Z') {
			s_priv.esc_z = true;
			s_priv.esc_kind = 'Z';
			s_priv.len_idx = 0U;
			s_priv.bulk_remain = 0U;
			return;
		}
		if (kind == 'H' || kind == 'W') {
			gs_stub_rx_push_str(ctx, "\x1bO");
		}
		return;
	}

	if (s_priv.line_len + 1U < sizeof(s_priv.line)) {
		s_priv.line[s_priv.line_len++] = (char)b;
	}
	if (b == '\n') {
		s_priv.line[s_priv.line_len] = '\0';
		stub_queue_response_for_cmd(ctx, s_priv.line);
		s_priv.line_len = 0;
	}
}

static int stub_uart_write(const uint8_t *data, size_t len, void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	size_t i;

	if (!ctx || !data) {
		return -1;
	}
	for (i = 0; i < len; i++) {
		if (ctx->tx_len < GS_STUB_TX_MAX) {
			ctx->tx[ctx->tx_len++] = data[i];
		}
		stub_on_tx_byte(ctx, data[i]);
	}
	return (int)len;
}

static int stub_uart_read(uint8_t *data, size_t max_len, uint32_t block_ms,
			  void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	size_t n = 0;

	(void)block_ms;
	if (!ctx || !data || max_len == 0U) {
		return 0;
	}
	while (n < max_len && ctx->rx_pos < ctx->rx_len) {
		data[n++] = ctx->rx[ctx->rx_pos++];
	}
	return (int)n;
}

static void stub_uart_flush(void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	if (!ctx) {
		return;
	}
	ctx->rx_pos = ctx->rx_len;
}

static uint32_t stub_millis(void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	return ctx ? ctx->now_ms : 0U;
}

static void stub_delay_ms(uint32_t ms, void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	if (ctx) {
		ctx->now_ms += ms;
	}
}

static void stub_reset_set(bool assert_reset, void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;

	if (!ctx) {
		return;
	}
	if (assert_reset) {
		s_priv.was_reset_asserted = true;
	} else if (s_priv.was_reset_asserted) {
		ctx->reset_pulses++;
		s_priv.was_reset_asserted = false;
		if (ctx->auto_ok) {
			gs_stub_rx_push_str(ctx, "Serial2WiFi APP\r\n");
		}
	}
}

static void stub_intf_sel_uart(void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	if (ctx) {
		ctx->last_intf_uart = true;
	}
}

static void stub_pgm_level_set(uint8_t level, void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	if (ctx) {
		ctx->last_pgm_assert = (level != 0U);
	}
}

static void stub_pgm_set(bool assert_pgm, void *vctx)
{
	stub_pgm_level_set(assert_pgm ? 1U : 0U, vctx);
}

static void stub_intf_sel_set(int level, void *vctx)
{
	gs_stub_ctx_t *ctx = (gs_stub_ctx_t *)vctx;
	if (ctx) {
		ctx->last_intf_uart = (level < 0) || (level == 1);
	}
}

static int stub_uart_set_baud(uint32_t baud, void *vctx)
{
	(void)baud;
	(void)vctx;
	return 0;
}

static void stub_ctrl_pins_get(gs_ctrl_pins_t *out, void *vctx)
{
	(void)vctx;
	if (!out) {
		return;
	}
	memset(out, 0, sizeof(*out));
	out->reset = 1U;
	out->pgm = 1U;
	out->intf_sel = 1U;
	out->intf_hiz = 1U;
}

void gs_stub_reset(gs_stub_ctx_t *ctx)
{
	if (!ctx) {
		return;
	}
	memset(ctx, 0, sizeof(*ctx));
	memset(&s_priv, 0, sizeof(s_priv));
	ctx->auto_ok = true;
}

void gs_stub_install(gs_stub_ctx_t *ctx, gs_platform_t *out)
{
	if (!ctx || !out) {
		return;
	}
	memset(out, 0, sizeof(*out));
	out->uart_write = stub_uart_write;
	out->uart_read = stub_uart_read;
	out->uart_flush = stub_uart_flush;
	out->uart_set_baud = stub_uart_set_baud;
	out->millis = stub_millis;
	out->delay_ms = stub_delay_ms;
	out->reset_set = stub_reset_set;
	out->intf_sel_uart = stub_intf_sel_uart;
	out->intf_sel_set = stub_intf_sel_set;
	out->pgm_set = stub_pgm_set;
	out->pgm_level_set = stub_pgm_level_set;
	out->ctrl_pins_get = stub_ctrl_pins_get;
	out->ctx = ctx;
	gs_platform_set(out);
}

void gs_stub_rx_push(gs_stub_ctx_t *ctx, const void *data, size_t len)
{
	const uint8_t *p = (const uint8_t *)data;
	size_t i;

	if (!ctx || !data) {
		return;
	}
	for (i = 0; i < len; i++) {
		if (ctx->rx_len < GS_STUB_RX_MAX) {
			ctx->rx[ctx->rx_len++] = p[i];
		}
	}
}

void gs_stub_rx_push_str(gs_stub_ctx_t *ctx, const char *s)
{
	if (!s) {
		return;
	}
	gs_stub_rx_push(ctx, s, strlen(s));
}

bool gs_stub_tx_contains(const gs_stub_ctx_t *ctx, const char *needle)
{
	size_t nlen;
	size_t i;

	if (!ctx || !needle) {
		return false;
	}
	nlen = strlen(needle);
	if (nlen == 0U || nlen > ctx->tx_len) {
		return false;
	}
	for (i = 0; i + nlen <= ctx->tx_len; i++) {
		if (memcmp(&ctx->tx[i], needle, nlen) == 0) {
			return true;
		}
	}
	return false;
}

const char *gs_stub_tx_cstr(gs_stub_ctx_t *ctx, char *buf, size_t buf_len)
{
	size_t n;

	if (!ctx || !buf || buf_len == 0U) {
		return "";
	}
	n = ctx->tx_len;
	if (n >= buf_len) {
		n = buf_len - 1U;
	}
	memcpy(buf, ctx->tx, n);
	buf[n] = '\0';
	return buf;
}
