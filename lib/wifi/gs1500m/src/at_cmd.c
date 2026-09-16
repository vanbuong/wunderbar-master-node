/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * AT command send / wait / ESC TX helpers.
 */

#include "gs1500m/at.h"
#include "gs1500m/platform.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int gs_at_write(const uint8_t *data, size_t len)
{
	const gs_platform_t *p = gs_platform_get();
	if (!p || !p->uart_write || !data) {
		return -1;
	}
	return p->uart_write(data, len, p->ctx);
}

static uint32_t now_ms(void)
{
	const gs_platform_t *p = gs_platform_get();
	if (!p || !p->millis) {
		return 0U;
	}
	return p->millis(p->ctx);
}

static void delay_ms(uint32_t ms)
{
	const gs_platform_t *p = gs_platform_get();
	if (p && p->delay_ms) {
		p->delay_ms(ms, p->ctx);
	}
}

gs_msg_id_t gs_at_wait_response(uint32_t timeout_ms)
{
	const gs_platform_t *p = gs_platform_get();
	uint32_t start = now_ms();
	uint8_t b;

	if (!p || !p->uart_read) {
		return GS_MSG_ERROR;
	}

	gs_at_clear_info_accum();

	for (;;) {
		int n = p->uart_read(&b, 1, 10U, p->ctx);
		if (n > 0) {
			gs_msg_id_t id = gs_at_process_byte(b);
			switch (id) {
			case GS_MSG_OK:
			case GS_MSG_ERROR:
			case GS_MSG_INVALID_INPUT:
			case GS_MSG_ERROR_IP_CONFIG:
			case GS_MSG_ERROR_SOCKET:
			case GS_MSG_CONNECT:
			case GS_MSG_CONNECT_SERVER_CLIENT:
			case GS_MSG_DISCONNECT:
			case GS_MSG_DISASSOCIATED:
			case GS_MSG_APP_RESET:
			case GS_MSG_ESC_OK:
			case GS_MSG_ESC_FAIL:
			case GS_MSG_FW_UPDATE_OK:
				return id;
			/*
			 * WELCOME ("Serial2WiFi APP") is a boot banner that can also
			 * appear in AT+VER output — do not treat it as a command end.
			 * APP_RESET ("UnExpected Warm Boot") is a real fault/reset.
			 */
			case GS_MSG_WELCOME:
			default:
				break;
			}
		}

		if ((now_ms() - start) >= timeout_ms) {
			return GS_MSG_TIMEOUT;
		}
		delay_ms(1);
	}
}

gs_msg_id_t gs_at_send_cmd(const char *cmd, uint32_t timeout_ms)
{
	size_t len;

	if (!cmd) {
		return GS_MSG_ERROR;
	}
	len = strlen(cmd);
	if (gs_at_write((const uint8_t *)cmd, len) < 0) {
		return GS_MSG_ERROR;
	}
	return gs_at_wait_response(timeout_ms);
}

gs_msg_id_t gs_at_send_cmdf(uint32_t timeout_ms, const char *fmt, ...)
{
	char buf[GS_AT_TX_CMD_MAX];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= sizeof(buf)) {
		return GS_MSG_ERROR;
	}
	return gs_at_send_cmd(buf, timeout_ms);
}

gs_msg_id_t gs_at_send_bulk(uint8_t cid, const uint8_t *data, size_t len,
			    uint32_t timeout_ms)
{
	char hdr[8];
	char digits[5];
	gs_msg_id_t id;

	if (!data && len > 0U) {
		return GS_MSG_ERROR;
	}
	if (len > 9999U) {
		return GS_MSG_INVALID_INPUT;
	}

	gs_at_u32_to_4digit((uint32_t)len, digits);
	hdr[0] = (char)GS_AT_ESC;
	hdr[1] = 'Z';
	hdr[2] = gs_at_cid_to_ascii(cid);
	hdr[3] = digits[0];
	hdr[4] = digits[1];
	hdr[5] = digits[2];
	hdr[6] = digits[3];

	if (gs_at_write((const uint8_t *)hdr, 7U) < 0) {
		return GS_MSG_ERROR;
	}
	if (len > 0U && gs_at_write(data, len) < 0) {
		return GS_MSG_ERROR;
	}

	id = gs_at_wait_response(timeout_ms);
	if (id == GS_MSG_ESC_OK || id == GS_MSG_OK) {
		return GS_MSG_OK;
	}
	return id;
}

gs_msg_id_t gs_at_send_stream(uint8_t cid, const uint8_t *data, size_t len,
			      uint32_t timeout_ms)
{
	uint8_t start[3];
	uint8_t end[2];
	gs_msg_id_t id;

	(void)timeout_ms;
	start[0] = GS_AT_ESC;
	start[1] = (uint8_t)'S';
	start[2] = (uint8_t)gs_at_cid_to_ascii(cid);
	end[0] = GS_AT_ESC;
	end[1] = (uint8_t)'E';

	if (gs_at_write(start, sizeof(start)) < 0) {
		return GS_MSG_ERROR;
	}
	if (len > 0U && data && gs_at_write(data, len) < 0) {
		return GS_MSG_ERROR;
	}
	if (gs_at_write(end, sizeof(end)) < 0) {
		return GS_MSG_ERROR;
	}
	/* Stream mode often has no immediate OK; treat write success as OK. */
	id = GS_MSG_OK;
	return id;
}
