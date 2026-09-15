/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/socket.h"

#include <stdio.h>

gs_msg_id_t gs_socket_tcp_client(const char *host, uint16_t port, uint8_t *cid_out)
{
	gs_msg_id_t id;

	if (!host) {
		return GS_MSG_INVALID_INPUT;
	}
	id = gs_at_send_cmdf(30000U, "AT+NCTCP=%s,%u\r\n", host, (unsigned)port);
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

	if (!host) {
		return GS_MSG_INVALID_INPUT;
	}
	id = gs_at_send_cmdf(30000U, "AT+NCUDP=%s,%u,%u\r\n", host, (unsigned)port,
			     (unsigned)local_port);
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
