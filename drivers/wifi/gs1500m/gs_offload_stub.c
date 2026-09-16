/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Stub net_offload for GS1500M v1 (wifi_mgmt only). Socket offload comes later.
 */

#include "gs.h"

#include <errno.h>

#include <zephyr/net/net_offload.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wifi_gs1500m);

static int gs_off_get(net_sa_family_t family, enum net_sock_type type,
		      enum net_ip_protocol ip_proto, struct net_context **context)
{
	ARG_UNUSED(family);
	ARG_UNUSED(type);
	ARG_UNUSED(ip_proto);
	ARG_UNUSED(context);
	return -ENOTSUP;
}

static int gs_off_bind(struct net_context *context, const struct net_sockaddr *addr,
		       net_socklen_t addrlen)
{
	ARG_UNUSED(context);
	ARG_UNUSED(addr);
	ARG_UNUSED(addrlen);
	return -ENOTSUP;
}

static int gs_off_listen(struct net_context *context, int backlog)
{
	ARG_UNUSED(context);
	ARG_UNUSED(backlog);
	return -ENOTSUP;
}

static int gs_off_connect(struct net_context *context, const struct net_sockaddr *addr,
			  net_socklen_t addrlen, net_context_connect_cb_t cb, int32_t timeout,
			  void *user_data)
{
	ARG_UNUSED(context);
	ARG_UNUSED(addr);
	ARG_UNUSED(addrlen);
	ARG_UNUSED(cb);
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int gs_off_accept(struct net_context *context, net_tcp_accept_cb_t cb, int32_t timeout,
			 void *user_data)
{
	ARG_UNUSED(context);
	ARG_UNUSED(cb);
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int gs_off_send(struct net_pkt *pkt, net_context_send_cb_t cb, int32_t timeout,
		       void *user_data)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(cb);
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int gs_off_sendto(struct net_pkt *pkt, const struct net_sockaddr *addr,
			 net_socklen_t addrlen, net_context_send_cb_t cb, int32_t timeout,
			 void *user_data)
{
	ARG_UNUSED(pkt);
	ARG_UNUSED(addr);
	ARG_UNUSED(addrlen);
	ARG_UNUSED(cb);
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int gs_off_recv(struct net_context *context, net_context_recv_cb_t cb, int32_t timeout,
		       void *user_data)
{
	ARG_UNUSED(context);
	ARG_UNUSED(cb);
	ARG_UNUSED(timeout);
	ARG_UNUSED(user_data);
	return -ENOTSUP;
}

static int gs_off_put(struct net_context *context)
{
	ARG_UNUSED(context);
	return -ENOTSUP;
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
