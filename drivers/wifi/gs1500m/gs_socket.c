/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * GS1500M socket table + ESC RX delivery into net_context.
 */

#include "gs.h"

#include <string.h>
#include <errno.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_ip.h>

LOG_MODULE_DECLARE(wifi_gs1500m);

static struct gs1500m_data *s_dev;

struct gs1500m_data *gs1500m_dev_data(void)
{
	return s_dev;
}

void gs1500m_sockets_init(struct gs1500m_data *data)
{
	int i;

	s_dev = data;
	for (i = 0; i < CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS; i++) {
		struct gs_socket *sock = &data->sockets[i];

		memset(sock, 0, sizeof(*sock));
		k_mutex_init(&sock->lock);
		k_sem_init(&sock->sem_data_ready, 0, 1);
		sock->cid = GS_AT_INVALID_CID;
	}
}

struct gs_socket *gs1500m_socket_alloc(struct gs1500m_data *data,
				       struct net_context *context)
{
	int i;

	for (i = 0; i < CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS; i++) {
		struct gs_socket *sock = &data->sockets[i];

		if (!gs_socket_flag(sock, GS_SOCK_IN_USE)) {
			gs_socket_flag_set(sock, GS_SOCK_IN_USE);
			sock->context = context;
			sock->cid = GS_AT_INVALID_CID;
			sock->recv_cb = NULL;
			sock->connect_cb = NULL;
			sock->rx_pkt = NULL;
			memset(&sock->src, 0, sizeof(sock->src));
			memset(&sock->dst, 0, sizeof(sock->dst));
			context->offload_context = sock;
			return sock;
		}
	}
	return NULL;
}

void gs1500m_socket_free(struct gs_socket *sock)
{
	if (!sock) {
		return;
	}
	k_mutex_lock(&sock->lock, K_FOREVER);
	if (sock->rx_pkt) {
		net_pkt_unref(sock->rx_pkt);
		sock->rx_pkt = NULL;
	}
	sock->context = NULL;
	sock->recv_cb = NULL;
	sock->connect_cb = NULL;
	sock->cid = GS_AT_INVALID_CID;
	gs_socket_flag_clear(sock, GS_SOCK_IN_USE | GS_SOCK_CONNECTED |
					   GS_SOCK_CONNECTING);
	k_mutex_unlock(&sock->lock);
}

struct gs_socket *gs1500m_socket_by_cid(struct gs1500m_data *data, uint8_t cid)
{
	int i;

	if (cid == GS_AT_INVALID_CID) {
		return NULL;
	}
	for (i = 0; i < CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS; i++) {
		struct gs_socket *sock = &data->sockets[i];

		if (gs_socket_flag(sock, GS_SOCK_IN_USE) && sock->cid == cid) {
			return sock;
		}
	}
	return NULL;
}

static void gs_on_data(uint8_t cid, gs_esc_kind_t kind, const uint8_t *data,
		       size_t len, void *user)
{
	struct gs1500m_data *dev = user;
	struct gs_socket *sock;

	ARG_UNUSED(kind);

	if (!dev || !data || len == 0U) {
		return;
	}
	sock = gs1500m_socket_by_cid(dev, cid);
	if (!sock || !gs_socket_flag(sock, GS_SOCK_CONNECTED)) {
		return;
	}

	k_mutex_lock(&sock->lock, K_FOREVER);
	if (!sock->rx_pkt) {
		sock->rx_pkt = net_pkt_rx_alloc_with_buffer(
			dev->iface, GS1500M_MTU, NET_AF_UNSPEC, 0, K_NO_WAIT);
		if (!sock->rx_pkt) {
			LOG_WRN("rx pkt alloc failed cid=%u", cid);
			k_mutex_unlock(&sock->lock);
			return;
		}
	}
	if (net_pkt_write(sock->rx_pkt, data, len) < 0) {
		LOG_WRN("rx pkt write failed cid=%u", cid);
		net_pkt_unref(sock->rx_pkt);
		sock->rx_pkt = NULL;
	}
	k_mutex_unlock(&sock->lock);
}

static void gs_on_data_end(uint8_t cid, gs_esc_kind_t kind, void *user)
{
	struct gs1500m_data *dev = user;
	struct gs_socket *sock;
	struct net_pkt *pkt;
	net_context_recv_cb_t cb;
	void *cb_data;

	ARG_UNUSED(kind);

	if (!dev) {
		return;
	}
	sock = gs1500m_socket_by_cid(dev, cid);
	if (!sock) {
		return;
	}

	k_mutex_lock(&sock->lock, K_FOREVER);
	pkt = sock->rx_pkt;
	sock->rx_pkt = NULL;
	cb = sock->recv_cb;
	cb_data = sock->recv_user_data;
	if (pkt) {
		net_pkt_set_context(pkt, sock->context);
		net_pkt_cursor_init(pkt);
	}
	k_mutex_unlock(&sock->lock);

	if (!pkt) {
		return;
	}
	if (cb) {
		cb(sock->context, pkt, NULL, NULL, 0, cb_data);
		k_sem_give(&sock->sem_data_ready);
	} else {
		net_pkt_unref(pkt);
	}
}

static void gs_on_line(gs_msg_id_t id, const char *line, void *user)
{
	struct gs1500m_data *dev = user;

	ARG_UNUSED(line);

	if (!dev) {
		return;
	}
	if (id == GS_MSG_DISCONNECT || id == GS_MSG_DISASSOCIATED) {
		gs1500m_sockets_notify_link_down(dev);
	}
}

void gs1500m_at_callbacks_install(struct gs1500m_data *data)
{
	gs_at_callbacks_t cbs = {
		.on_data = gs_on_data,
		.on_data_end = gs_on_data_end,
		.on_line = gs_on_line,
		.user = data,
	};

	gs_at_set_callbacks(&cbs);
}

void gs1500m_sockets_notify_link_down(struct gs1500m_data *data)
{
	int i;

	for (i = 0; i < CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS; i++) {
		struct gs_socket *sock = &data->sockets[i];
		net_context_recv_cb_t cb;
		void *cb_data;

		if (!gs_socket_flag(sock, GS_SOCK_IN_USE)) {
			continue;
		}
		gs_socket_flag_clear(sock, GS_SOCK_CONNECTED);

		k_mutex_lock(&sock->lock, K_FOREVER);
		if (sock->rx_pkt) {
			net_pkt_unref(sock->rx_pkt);
			sock->rx_pkt = NULL;
		}
		cb = sock->recv_cb;
		cb_data = sock->recv_user_data;
		k_mutex_unlock(&sock->lock);

		if (cb) {
			cb(sock->context, NULL, NULL, NULL, 0, cb_data);
			k_sem_give(&sock->sem_data_ready);
		}
	}
}

void gs1500m_parser_drain_locked(struct gs1500m_data *data)
{
	const gs_platform_t *p = gs_platform_get();
	uint8_t b;
	int n;

	ARG_UNUSED(data);

	if (!p || !p->uart_read) {
		return;
	}

	/* Poll UART HW into ring when IRQ RX is off. */
	gs_platform_zephyr_rx_pump(0);

	while ((n = p->uart_read(&b, 1, 0U, p->ctx)) > 0) {
		gs_msg_id_t id = gs_at_process_byte(b);

		if (id == GS_MSG_DISCONNECT || id == GS_MSG_DISASSOCIATED) {
			gs1500m_sockets_notify_link_down(data);
		}
	}
}
