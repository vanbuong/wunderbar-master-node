/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/http.h"

#include <stdio.h>

gs_msg_id_t gs_http_open(const char *host, uint16_t port, bool tls,
			 const char *cacert_or_null, uint8_t *cid_out)
{
	gs_msg_id_t id;

	if (!host) {
		return GS_MSG_INVALID_INPUT;
	}

	if (tls) {
		const char *ca = cacert_or_null ? cacert_or_null : "";
		id = gs_at_send_cmdf(30000U, "AT+HTTPOPEN=%s,%u,1,%s\r\n", host,
				     (unsigned)port, ca);
	} else {
		id = gs_at_send_cmdf(30000U, "AT+HTTPOPEN=%s,%u\r\n", host,
				     (unsigned)port);
	}

	if (cid_out) {
		*cid_out = gs_at_parse_connect_cid();
	}
	return id;
}

gs_msg_id_t gs_http_send(uint8_t cid, int method, uint32_t timeout_s,
			 const char *path, const uint8_t *body, size_t body_len)
{
	gs_msg_id_t id;
	char digits[5];
	uint8_t hdr[8];

	if (!path) {
		return GS_MSG_INVALID_INPUT;
	}

	if (body_len > 0U) {
		id = gs_at_send_cmdf(timeout_s * 1000U,
				     "AT+HTTPSEND=%c,%d,%u,%s,%u\r\n",
				     gs_at_cid_to_ascii(cid), method,
				     (unsigned)timeout_s, path,
				     (unsigned)body_len);
	} else {
		id = gs_at_send_cmdf(timeout_s * 1000U,
				     "AT+HTTPSEND=%c,%d,%u,%s\r\n",
				     gs_at_cid_to_ascii(cid), method,
				     (unsigned)timeout_s, path);
	}
	if (id != GS_MSG_OK) {
		return id;
	}

	if (body_len == 0U) {
		return id;
	}

	gs_at_u32_to_4digit((uint32_t)body_len, digits);
	hdr[0] = GS_AT_ESC;
	hdr[1] = (uint8_t)'H';
	hdr[2] = (uint8_t)gs_at_cid_to_ascii(cid);
	hdr[3] = (uint8_t)digits[0];
	hdr[4] = (uint8_t)digits[1];
	hdr[5] = (uint8_t)digits[2];
	hdr[6] = (uint8_t)digits[3];
	if (gs_at_write(hdr, 7U) < 0) {
		return GS_MSG_ERROR;
	}
	if (gs_at_write(body, body_len) < 0) {
		return GS_MSG_ERROR;
	}
	return gs_at_wait_response(timeout_s * 1000U);
}

gs_msg_id_t gs_http_close(uint8_t cid)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+HTTPCLOSE=%c\r\n",
			       gs_at_cid_to_ascii(cid));
}

gs_msg_id_t gs_http_conf(int param, const char *value)
{
	if (!value) {
		return GS_MSG_INVALID_INPUT;
	}
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+HTTPCONF=%d,%s\r\n",
			       param, value);
}
