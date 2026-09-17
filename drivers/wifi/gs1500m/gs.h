/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * GainSpan GS1500M WiFi offload — private driver state.
 */

#ifndef ZEPHYR_DRIVERS_WIFI_GS1500M_GS_H_
#define ZEPHYR_DRIVERS_WIFI_GS1500M_GS_H_

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/drivers/gpio.h>

#include "gs1500m/wifi.h"
#include "gs1500m/socket.h"
#include "gs1500m/at.h"
#include "gs_platform_zephyr.h"

#define GS1500M_MTU 1500

#ifndef CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS
#define CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS 4
#endif

#define GS_SOCK_IN_USE      BIT(0)
#define GS_SOCK_CONNECTED   BIT(1)
#define GS_SOCK_CONNECTING  BIT(2)

struct gs1500m_config {
	const struct gpio_dt_spec reset;
	const struct gpio_dt_spec pgm;
	const struct gpio_dt_spec intf_sel;
	uint32_t target_speed;
};

struct gs_socket {
	struct k_mutex lock;
	atomic_t flags;
	uint8_t cid; /* GS_AT_INVALID_CID until CONNECT */

	struct net_sockaddr src;
	struct net_sockaddr dst;

	struct net_context *context;
	net_context_recv_cb_t recv_cb;
	net_context_connect_cb_t connect_cb;
	void *recv_user_data;
	void *conn_user_data;

	struct k_sem sem_data_ready;

	/* RX reassembly across ESC chunk callbacks */
	struct net_pkt *rx_pkt;
};

struct gs1500m_data {
	struct net_if *iface;
	gs_platform_t plat;

	struct k_mutex lock;
	struct k_work_q workq;
	struct k_work init_work;
	struct k_work connect_work;
	struct k_work disconnect_work;

	struct wifi_connect_req_params conn;
	char ssid[WIFI_SSID_MAX_LEN + 1];
	char psk[64];

	bool initialized;
	bool connected;
	bool connecting;

	struct gs_socket sockets[CONFIG_WIFI_GS1500M_AT_MAX_SOCKETS];

	struct k_thread rx_thread;
	k_thread_stack_t *rx_stack;
};

struct gs1500m_data *gs1500m_dev_data(void);

void gs1500m_sockets_init(struct gs1500m_data *data);
struct gs_socket *gs1500m_socket_alloc(struct gs1500m_data *data,
				       struct net_context *context);
void gs1500m_socket_free(struct gs_socket *sock);
struct gs_socket *gs1500m_socket_by_cid(struct gs1500m_data *data, uint8_t cid);

void gs1500m_at_callbacks_install(struct gs1500m_data *data);
void gs1500m_parser_drain_locked(struct gs1500m_data *data);
void gs1500m_sockets_notify_link_down(struct gs1500m_data *data);

int gs1500m_offload_init(struct net_if *iface);

static inline bool gs_socket_flag(struct gs_socket *sock, atomic_val_t f)
{
	return (atomic_get(&sock->flags) & f) != 0;
}

static inline void gs_socket_flag_set(struct gs_socket *sock, atomic_val_t f)
{
	atomic_or(&sock->flags, f);
}

static inline void gs_socket_flag_clear(struct gs_socket *sock, atomic_val_t f)
{
	atomic_and(&sock->flags, ~f);
}

#endif /* ZEPHYR_DRIVERS_WIFI_GS1500M_GS_H_ */
