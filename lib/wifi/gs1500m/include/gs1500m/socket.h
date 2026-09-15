/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef GS1500M_SOCKET_H
#define GS1500M_SOCKET_H

#include "gs1500m/at.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

gs_msg_id_t gs_socket_tcp_client(const char *host, uint16_t port, uint8_t *cid_out);
gs_msg_id_t gs_socket_udp_client(const char *host, uint16_t port,
				 uint16_t local_port, uint8_t *cid_out);
gs_msg_id_t gs_socket_tcp_server(uint16_t port, uint8_t *cid_out);
gs_msg_id_t gs_socket_udp_server(uint16_t port, uint8_t *cid_out);
gs_msg_id_t gs_socket_close(uint8_t cid);
gs_msg_id_t gs_socket_close_all(void);

/** Send via ESC Z bulk (preferred with BDATA=1). */
gs_msg_id_t gs_socket_send(uint8_t cid, const uint8_t *data, size_t len);

/** Send via ESC S stream. */
gs_msg_id_t gs_socket_send_stream(uint8_t cid, const uint8_t *data, size_t len);

gs_msg_id_t gs_socket_cid_list(void);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_SOCKET_H */
