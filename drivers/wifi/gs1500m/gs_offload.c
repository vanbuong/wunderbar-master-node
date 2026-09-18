/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr net_offload for GS1500M (IPv4 TCP/UDP client).
 */

#include "gs.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <zephyr/logging/log.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_context.h>

LOG_MODULE_DECLARE(wifi_gs1500m);

#ifndef CONFIG_WIFI_GS1500M_AT_TX_BUF_SIZE
#define CONFIG_WIFI_GS1500M_AT_TX_BUF_SIZE 1500
#endif

static int gs_off_listen(struct net_context *context, int backlog)
{
	ARG_UNUSED(context);
	ARG_UNUSED(backlog);
	return -ENOTSUP;
}

static int gs_off_accept(struct net_context *context, net_tcp_accept_cb_t cb,
			 int32_t timeout, void *user_data)
{
	ARG_UNUSED(context);
	ARG_UNUSED(cb);
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int gs_off_get(net_sa_family_t family, enum net_sock_type type,
		      enum net_ip_protocol ip_proto, struct net_context **context)
{
	struct gs1500m_data *data = gs1500m_dev_data();
	struct gs_socket *sock;

	ARG_UNUSED(type);
	ARG_UNUSED(ip_proto);

	if (!data || !context || !*context) {
		return -EINVAL;
	}
	if (family != NET_AF_INET) {
		return -EAFNOSUPPORT;
	}

	sock = gs1500m_socket_alloc(data, *context);
	if (!sock) {
		return -ENOMEM;
	}
	LOG_DBG("socket allocated");
	return 0;
}

static int gs_off_bind(struct net_context *context, const struct net_sockaddr *addr,
		       net_socklen_t addrlen)
{
	struct gs_socket *sock = context->offload_context;

	ARG_UNUSED(addrlen);

	if (!sock) {
		return -EINVAL;
	}
	if (net_context_get_proto(context) == NET_IPPROTO_TCP) {
		return 0;
	}
	if (!addr || addr->sa_family != NET_AF_INET) {
		return -EAFNOSUPPORT;
	}
	if (gs_socket_flag(sock, GS_SOCK_CONNECTED)) {
		return -EISCONN;
	}
	k_mutex_lock(&sock->lock, K_FOREVER);
	sock->src = *addr;
	k_mutex_unlock(&sock->lock);
	return 0;
}

static int gs_sock_connect_locked(struct gs1500m_data *data, struct gs_socket *sock)
{
	char host[NET_IPV4_ADDR_LEN];
	uint16_t port;
	uint16_t lport = 0U;
	uint8_t cid = GS_AT_INVALID_CID;
	gs_msg_id_t id;
	struct net_sockaddr dst;
	struct net_sockaddr src;

	if (!data->connected) {
		return -ENETUNREACH;
	}

	k_mutex_lock(&sock->lock, K_FOREVER);
	dst = sock->dst;
	src = sock->src;
	k_mutex_unlock(&sock->lock);

	if (dst.sa_family != NET_AF_INET) {
		return -EAFNOSUPPORT;
	}
	if (net_addr_ntop(NET_AF_INET, &net_sin(&dst)->sin_addr, host,
			  sizeof(host)) == NULL) {
		return -EINVAL;
	}
	port = net_ntohs(net_sin(&dst)->sin_port);
	if (src.sa_family == NET_AF_INET) {
		lport = net_ntohs(net_sin(&src)->sin_port);
	}

	gs_socket_flag_set(sock, GS_SOCK_CONNECTING);

	if (net_context_get_proto(sock->context) == NET_IPPROTO_TCP) {
		LOG_INF("NCTCP %s:%u", host, port);
		id = gs_socket_tcp_client(host, port, &cid);
	} else {
		if (lport == 0U) {
			lport = 50000U + (uint16_t)(k_uptime_get_32() & 0x3ffU);
		}
		LOG_INF("NCUDP %s:%u lport=%u", host, port, lport);
		id = gs_socket_udp_client(host, port, lport, &cid);
	}

	gs_socket_flag_clear(sock, GS_SOCK_CONNECTING);

	if (id != GS_MSG_CONNECT && id != GS_MSG_OK) {
		const char *last = gs_at_last_line();
		const char *partial = gs_at_partial_line();

		LOG_ERR("socket open failed id=%d (%s) last='%s' partial='%s'",
			(int)id, gs_at_msg_name(id), last ? last : "",
			partial ? partial : "");
		return -EIO;
	}
	if (cid == GS_AT_INVALID_CID) {
		LOG_ERR("socket open: no CID");
		return -EIO;
	}

	sock->cid = cid;
	gs_socket_flag_set(sock, GS_SOCK_CONNECTED);
	if (net_context_get_proto(sock->context) == NET_IPPROTO_TCP) {
		net_context_set_state(sock->context, NET_CONTEXT_CONNECTED);
	}
	LOG_INF("socket cid=%u connected", cid);
	return 0;
}

static int gs_off_connect(struct net_context *context, const struct net_sockaddr *addr,
			  net_socklen_t addrlen, net_context_connect_cb_t cb,
			  int32_t timeout, void *user_data)
{
	struct gs_socket *sock = context->offload_context;
	struct gs1500m_data *data = gs1500m_dev_data();
	int ret;

	ARG_UNUSED(addrlen);
	ARG_UNUSED(timeout);

	if (!sock || !data || !addr) {
		return -EINVAL;
	}
	if (addr->sa_family != NET_AF_INET) {
		return -EAFNOSUPPORT;
	}
	if (gs_socket_flag(sock, GS_SOCK_CONNECTED)) {
		return -EISCONN;
	}

	k_mutex_lock(&sock->lock, K_FOREVER);
	sock->dst = *addr;
	sock->connect_cb = cb;
	sock->conn_user_data = user_data;
	k_mutex_unlock(&sock->lock);

	k_mutex_lock(&data->lock, K_FOREVER);
	ret = gs_sock_connect_locked(data, sock);
	k_mutex_unlock(&data->lock);

	if (cb) {
		cb(context, ret, user_data);
	}
	return ret;
}

static int gs_pkt_to_buf(struct net_pkt *pkt, uint8_t *buf, size_t buf_len,
			 size_t *out_len)
{
	size_t len = net_pkt_get_len(pkt);

	if (len == 0U) {
		*out_len = 0U;
		return 0;
	}
	if (len > buf_len || len > 9999U) {
		return -EMSGSIZE;
	}
	net_pkt_cursor_init(pkt);
	if (net_pkt_read(pkt, buf, len) < 0) {
		return -EIO;
	}
	*out_len = len;
	return 0;
}

static int gs_off_sendto(struct net_pkt *pkt, const struct net_sockaddr *addr,
			 net_socklen_t addrlen, net_context_send_cb_t cb,
			 int32_t timeout, void *user_data)
{
	struct net_context *context = pkt->context;
	struct gs_socket *sock;
	struct gs1500m_data *data = gs1500m_dev_data();
	static uint8_t tx_buf[CONFIG_WIFI_GS1500M_AT_TX_BUF_SIZE];
	size_t len = 0U;
	gs_msg_id_t id;
	int ret;

	ARG_UNUSED(addr);
	ARG_UNUSED(addrlen);
	ARG_UNUSED(timeout);

	if (!context || !data) {
		ret = -EINVAL;
		goto out;
	}
	sock = context->offload_context;
	if (!sock || !gs_socket_flag(sock, GS_SOCK_CONNECTED)) {
		ret = -ENOTCONN;
		goto out;
	}

	ret = gs_pkt_to_buf(pkt, tx_buf, sizeof(tx_buf), &len);
	if (ret) {
		goto out;
	}
	if (len == 0U) {
		ret = 0;
		goto out;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	id = gs_socket_send(sock->cid, tx_buf, len);
	k_mutex_unlock(&data->lock);

	ret = (id == GS_MSG_OK) ? 0 : -EIO;
out:
	if (cb) {
		cb(context, ret, user_data);
	}
	if (ret == 0) {
		net_pkt_unref(pkt);
	}
	return ret;
}

static int gs_off_send(struct net_pkt *pkt, net_context_send_cb_t cb, int32_t timeout,
		       void *user_data)
{
	return gs_off_sendto(pkt, NULL, 0, cb, timeout, user_data);
}

static int gs_off_recv(struct net_context *context, net_context_recv_cb_t cb,
		       int32_t timeout, void *user_data)
{
	struct gs_socket *sock = context->offload_context;
	int ret = 0;

	if (!sock) {
		return -EINVAL;
	}

	k_mutex_lock(&sock->lock, K_FOREVER);
	sock->recv_cb = cb;
	sock->recv_user_data = user_data;
	k_sem_reset(&sock->sem_data_ready);
	k_mutex_unlock(&sock->lock);

	if (timeout == 0) {
		return 0;
	}
	if (timeout < 0) {
		ret = k_sem_take(&sock->sem_data_ready, K_FOREVER);
	} else {
		ret = k_sem_take(&sock->sem_data_ready, K_MSEC(timeout));
	}
	return (ret == 0) ? 0 : -ETIMEDOUT;
}

static int gs_off_put(struct net_context *context)
{
	struct gs_socket *sock = context->offload_context;
	struct gs1500m_data *data = gs1500m_dev_data();
	uint8_t cid;

	if (!sock || !data) {
		return -EINVAL;
	}

	cid = sock->cid;
	if (cid != GS_AT_INVALID_CID && gs_socket_flag(sock, GS_SOCK_CONNECTED)) {
		k_mutex_lock(&data->lock, K_FOREVER);
		(void)gs_socket_close(cid);
		k_mutex_unlock(&data->lock);
	}
	gs_socket_flag_clear(sock, GS_SOCK_CONNECTED);
	gs1500m_socket_free(sock);
	context->offload_context = NULL;
	return 0;
}

static struct net_offload gs_offload = {
	.get = gs_off_get,
	.bind = gs_off_bind,
	.listen = gs_off_listen,
	.connect = gs_off_connect,
	.accept = gs_off_accept,
	.send = gs_off_send,
	.sendto = gs_off_sendto,
	.recv = gs_off_recv,
	.put = gs_off_put,
};

int gs1500m_offload_init(struct net_if *iface)
{
	net_if_offload_set(iface, &gs_offload);
	return 0;
}
