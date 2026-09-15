/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/ssl.h"

#include <stdio.h>

gs_msg_id_t gs_ssl_cert_add(const char *name, bool to_flash,
			    const uint8_t *der, size_t len)
{
	gs_msg_id_t id;
	uint8_t esc_w[2] = { GS_AT_ESC, (uint8_t)'W' };

	if (!name || (!der && len > 0U)) {
		return GS_MSG_INVALID_INPUT;
	}

	/* AT+TCERTADD=<name>,0,<len>,<ram=1/flash=0> */
	id = gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS,
			     "AT+TCERTADD=%s,0,%u,%u\r\n", name, (unsigned)len,
			     to_flash ? 0U : 1U);
	if (id != GS_MSG_OK) {
		return id;
	}

	if (gs_at_write(esc_w, sizeof(esc_w)) < 0) {
		return GS_MSG_ERROR;
	}
	if (len > 0U && gs_at_write(der, len) < 0) {
		return GS_MSG_ERROR;
	}
	id = gs_at_wait_response(GS_AT_DEFAULT_CMD_TIMEOUT_MS);
	if (id == GS_MSG_ESC_OK || id == GS_MSG_OK) {
		return GS_MSG_OK;
	}
	return id;
}

gs_msg_id_t gs_ssl_cert_delete(const char *name)
{
	if (!name) {
		return GS_MSG_INVALID_INPUT;
	}
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+TCERTDEL=%s\r\n",
			       name);
}

gs_msg_id_t gs_ssl_open(uint8_t cid, const char *cacert,
			const char *client_cert, const char *client_key)
{
	const char *ca = cacert ? cacert : "";

	if (client_cert || client_key) {
		return gs_at_send_cmdf(
			30000U, "AT+SSLOPEN=%c,%s,%s,%s\r\n",
			gs_at_cid_to_ascii(cid), ca,
			client_cert ? client_cert : "",
			client_key ? client_key : "");
	}
	return gs_at_send_cmdf(30000U, "AT+SSLOPEN=%c,%s\r\n",
			       gs_at_cid_to_ascii(cid), ca);
}

gs_msg_id_t gs_ssl_close(uint8_t cid)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+SSLCLOSE=%c\r\n",
			       gs_at_cid_to_ascii(cid));
}
