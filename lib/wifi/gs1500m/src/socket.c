/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/socket.h"
#include "gs1500m/platform.h"

#include <stdio.h>

static void delay_ms(uint32_t ms)
{
	const gs_platform_t *p = gs_platform_get();

	if (p && p->delay_ms) {
		p->delay_ms(ms, p->ctx);
	}
}

/*
 * Drop stale UART bytes before opening a client socket.
 * Async SNTP (NTIMESYNC) can leave a late "ERROR: SOCKET FAILURE" in the
 * RX ring; without a flush the next NCTCP wait returns that error in ~ms.
 */
static void socket_open_prepare(void)
{
	gs_at_flush();
}

static bool should_retry_open(gs_msg_id_t id)
{
	return id == GS_MSG_ERROR_SOCKET || id == GS_MSG_ERROR ||
	       id == GS_MSG_TIMEOUT;
}

static void socket_open_recover(void)
{
	gs_at_flush();
	(void)gs_socket_close_all();
	delay_ms(200);
	gs_at_flush();
}

gs_msg_id_t gs_socket_tcp_client(const char *host, uint16_t port, uint8_t *cid_out)
{
	gs_msg_id_t id;
	int attempt;

	if (!host) {
		return GS_MSG_INVALID_INPUT;
	}

	for (attempt = 0; attempt < 2; attempt++) {
		if (attempt == 0) {
			socket_open_prepare();
		} else {
			socket_open_recover();
		}
		id = gs_at_send_cmdf(30000U, "AT+NCTCP=%s,%u\r\n", host,
				     (unsigned)port);
		if (id == GS_MSG_CONNECT || id == GS_MSG_OK) {
			break;
		}
		if (attempt == 0 && should_retry_open(id)) {
			continue;
		}
		break;
	}

	if (cid_out) {
		*cid_out = (id == GS_MSG_CONNECT || id == GS_MSG_OK)
				   ? gs_at_parse_connect_cid()
				   : GS_AT_INVALID_CID;
	}
	return id;
}

gs_msg_id_t gs_socket_udp_client(const char *host, uint16_t port,
				 uint16_t local_port, uint8_t *cid_out)
{
	gs_msg_id_t id;
	int attempt;

	if (!host) {
		return GS_MSG_INVALID_INPUT;
	}

	for (attempt = 0; attempt < 2; attempt++) {
		if (attempt == 0) {
			socket_open_prepare();
		} else {
			socket_open_recover();
		}
		id = gs_at_send_cmdf(30000U, "AT+NCUDP=%s,%u,%u\r\n", host,
				     (unsigned)port, (unsigned)local_port);
		if (id == GS_MSG_CONNECT || id == GS_MSG_OK) {
			break;
		}
		if (attempt == 0 && should_retry_open(id)) {
			continue;
		}
		break;
	}

	if (cid_out) {
		*cid_out = (id == GS_MSG_CONNECT || id == GS_MSG_OK)
				   ? gs_at_parse_connect_cid()
				   : GS_AT_INVALID_CID;
	}
	return id;
}

gs_msg_id_t gs_socket_tcp_server(uint16_t port, uint8_t *cid_out)
{
	gs_msg_id_t id = gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS,
					 "AT+NSTCP=%u\r\n", (unsigned)port);
	if (cid_out) {
		*cid_out = gs_at_parse_connect_cid();
	}
	return id;
}

gs_msg_id_t gs_socket_udp_server(uint16_t port, uint8_t *cid_out)
{
	gs_msg_id_t id = gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS,
					 "AT+NSUDP=%u\r\n", (unsigned)port);
	if (cid_out) {
		*cid_out = gs_at_parse_connect_cid();
	}
	return id;
}

gs_msg_id_t gs_socket_close(uint8_t cid)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+NCLOSE=%c\r\n",
			       gs_at_cid_to_ascii(cid));
}

gs_msg_id_t gs_socket_close_all(void)
{
	return gs_at_send_cmd("AT+NCLOSEALL\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}

gs_msg_id_t gs_socket_send(uint8_t cid, const uint8_t *data, size_t len)
{
	return gs_at_send_bulk(cid, data, len, GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}

gs_msg_id_t gs_socket_send_stream(uint8_t cid, const uint8_t *data, size_t len)
{
	return gs_at_send_stream(cid, data, len, GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}

gs_msg_id_t gs_socket_cid_list(void)
{
	return gs_at_send_cmd("AT+CID=?\r\n", GS_AT_DEFAULT_CMD_TIMEOUT_MS);
}
