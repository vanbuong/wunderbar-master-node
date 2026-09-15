/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/mqtt_pipe.h"
#include "gs1500m/socket.h"
#include "gs1500m/ssl.h"

#include <string.h>

gs_msg_id_t gs_mqtt_pipe_open(gs_mqtt_pipe_t *pipe, const char *host,
			      uint16_t port, bool tls, const char *cacert)
{
	gs_msg_id_t id;
	uint8_t cid = GS_AT_INVALID_CID;

	if (!pipe || !host) {
		return GS_MSG_INVALID_INPUT;
	}
	memset(pipe, 0, sizeof(*pipe));

	id = gs_socket_tcp_client(host, port, &cid);
	if (id != GS_MSG_CONNECT && id != GS_MSG_OK) {
		return id;
	}
	if (cid == GS_AT_INVALID_CID) {
		return GS_MSG_ERROR_SOCKET;
	}

	if (tls) {
		id = gs_ssl_open(cid, cacert ? cacert : "", NULL, NULL);
		if (id != GS_MSG_OK) {
			(void)gs_socket_close(cid);
			return id;
		}
		pipe->tls = true;
	}

	pipe->cid = cid;
	pipe->open = true;
	return GS_MSG_OK;
}

gs_msg_id_t gs_mqtt_pipe_send(gs_mqtt_pipe_t *pipe, const uint8_t *data,
			      size_t len)
{
	if (!pipe || !pipe->open) {
		return GS_MSG_ERROR;
	}
	return gs_socket_send(pipe->cid, data, len);
}

gs_msg_id_t gs_mqtt_pipe_close(gs_mqtt_pipe_t *pipe)
{
	gs_msg_id_t id = GS_MSG_OK;

	if (!pipe) {
		return GS_MSG_INVALID_INPUT;
	}
	if (pipe->open) {
		if (pipe->tls) {
			(void)gs_ssl_close(pipe->cid);
		}
		id = gs_socket_close(pipe->cid);
		pipe->open = false;
	}
	return id;
}
