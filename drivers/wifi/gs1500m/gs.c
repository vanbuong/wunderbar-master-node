/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * GainSpan GS1500M WiFi offload driver (wifi_mgmt v1).
 * AT transport via portable lib/wifi/gs1500m + Zephyr UART port.
 */

#define DT_DRV_COMPAT gainspan_gs1500m_at

#include "gs.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_offload.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/conn_mgr/connectivity_wifi_mgmt.h>
#include <zephyr/net/net_ip.h>

#include "wb_time.h"

LOG_MODULE_REGISTER(wifi_gs1500m, CONFIG_WIFI_LOG_LEVEL);

#define GS_WORKQ_STACK_SIZE CONFIG_WIFI_GS1500M_AT_WORKQ_STACK_SIZE
#define GS_RX_STACK_SIZE    CONFIG_WIFI_GS1500M_AT_RX_STACK_SIZE

K_KERNEL_STACK_DEFINE(gs_workq_stack, GS_WORKQ_STACK_SIZE);
K_KERNEL_STACK_DEFINE(gs_rx_stack, GS_RX_STACK_SIZE);

static struct gs1500m_data gs_data;
static const struct gs1500m_config gs_config = {
	.reset = GPIO_DT_SPEC_INST_GET_OR(0, reset_gpios, { 0 }),
	.pgm = GPIO_DT_SPEC_INST_GET_OR(0, pgm_gpios, { 0 }),
	.intf_sel = GPIO_DT_SPEC_INST_GET_OR(0, intf_sel_gpios, { 0 }),
	.target_speed = DT_INST_PROP_OR(0, target_speed, 115200),
};

static void gs_apply_ipv4_from_status(struct gs1500m_data *data,
				      const gs_wifi_status_t *st)
{
	struct net_in_addr addr;
	struct net_in_addr gw;
	struct net_in_addr nm;

	if (!data->iface || !st || !st->ip[0]) {
		return;
	}

	if (net_addr_pton(NET_AF_INET, st->ip, &addr) == 0) {
		(void)net_if_ipv4_addr_add(data->iface, &addr, NET_ADDR_DHCP, 0);
	}
	if (st->gateway[0] && net_addr_pton(NET_AF_INET, st->gateway, &gw) == 0) {
		net_if_ipv4_set_gw(data->iface, &gw);
	}
	if (st->subnet[0] && net_addr_pton(NET_AF_INET, st->subnet, &nm) == 0) {
		(void)net_if_ipv4_set_netmask_by_addr(data->iface, &addr, &nm);
	}
}

static void gs_rx_thread_fn(void *p1, void *p2, void *p3)
{
	struct gs1500m_data *data = p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		if (k_mutex_lock(&data->lock, K_MSEC(20)) == 0) {
			if (data->initialized) {
				gs_platform_zephyr_rx_poll(5);
			}
			k_mutex_unlock(&data->lock);
		}
		k_msleep(10);
	}
}

static void gs_init_work_fn(struct k_work *work)
{
	struct gs1500m_data *data = CONTAINER_OF(work, struct gs1500m_data, init_work);
	gs_msg_id_t id;
	const gs_wifi_module_info_t *mi;

	k_mutex_lock(&data->lock, K_FOREVER);

	if (gs_platform_zephyr_init(&data->plat) != 0) {
		LOG_ERR("platform init failed");
		k_mutex_unlock(&data->lock);
		return;
	}

	wb_time_init(NULL, NULL);

	id = gs_wifi_init(CONFIG_WIFI_GS1500M_AT_INIT_TIMEOUT_MS);
	if (id != GS_MSG_OK) {
		LOG_ERR("gs_wifi_init failed (%d)", (int)id);
		k_mutex_unlock(&data->lock);
		return;
	}

	(void)gs_wifi_query_module_info(NULL);
	mi = gs_wifi_last_module_info();
	if (mi && mi->mac[0] && data->iface) {
		uint8_t mac[6];
		unsigned a, b, c, d, e, f;

		if (sscanf(mi->mac, "%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f) == 6) {
			mac[0] = (uint8_t)a;
			mac[1] = (uint8_t)b;
			mac[2] = (uint8_t)c;
			mac[3] = (uint8_t)d;
			mac[4] = (uint8_t)e;
			mac[5] = (uint8_t)f;
			net_if_set_link_addr(data->iface, mac, sizeof(mac),
					     NET_LINK_ETHERNET);
		}
		LOG_INF("GS1500M ready mac=%s", mi->mac);
	} else {
		LOG_INF("GS1500M ready");
	}

	data->initialized = true;
	if (data->iface) {
		net_if_carrier_on(data->iface);
	}

	k_mutex_unlock(&data->lock);
}

static void gs_connect_work_fn(struct k_work *work)
{
	struct gs1500m_data *data =
		CONTAINER_OF(work, struct gs1500m_data, connect_work);
	gs_msg_id_t id;
	gs_wifi_status_t st;
	int status = 0;

	k_mutex_lock(&data->lock, K_FOREVER);
	data->connecting = true;

	id = gs_wifi_join_wpa(data->ssid, data->psk[0] ? data->psk : NULL);
	if (id == GS_MSG_APP_RESET || id == GS_MSG_TIMEOUT) {
		gs_at_flush();
		if (gs_wifi_init(CONFIG_WIFI_GS1500M_AT_INIT_TIMEOUT_MS) == GS_MSG_OK) {
			id = gs_wifi_join_wpa(data->ssid,
					      data->psk[0] ? data->psk : NULL);
		}
	}

	if (id == GS_MSG_OK) {
		(void)gs_wifi_bulk_data(true);
		(void)gs_wifi_query_module_info(NULL);
		memset(&st, 0, sizeof(st));
		(void)gs_wifi_get_status(&st);
		gs_apply_ipv4_from_status(data, &st);
		data->connected = true;
		if (data->iface) {
			net_if_dormant_off(data->iface);
		}
		LOG_INF("associated ssid=%s ip=%s", data->ssid,
			st.ip[0] ? st.ip : "(none)");

		/* Best-effort module SNTP → SYS_CLOCK_REALTIME */
		{
			uint32_t unix_sec = 0U;

			if (gs_wifi_ntp_sync(&unix_sec, NULL, 0U) == GS_MSG_OK &&
			    unix_sec != 0U) {
				wb_time_set_unix(unix_sec);
				LOG_INF("time synced unix=%u", (unsigned)unix_sec);
			}
		}
		status = 0;
	} else {
		data->connected = false;
		status = -EIO;
		LOG_ERR("join failed (%d)", (int)id);
	}

	data->connecting = false;
	k_mutex_unlock(&data->lock);

	if (data->iface) {
		wifi_mgmt_raise_connect_result_event(data->iface, status);
	}
}

static void gs_disconnect_work_fn(struct k_work *work)
{
	struct gs1500m_data *data =
		CONTAINER_OF(work, struct gs1500m_data, disconnect_work);

	k_mutex_lock(&data->lock, K_FOREVER);
	(void)gs_wifi_disconnect();
	data->connected = false;
	if (data->iface) {
		net_if_dormant_on(data->iface);
	}
	k_mutex_unlock(&data->lock);

	if (data->iface) {
		wifi_mgmt_raise_disconnect_result_event(data->iface, 0);
	}
}

static int gs_mgmt_connect(const struct device *dev,
			   struct wifi_connect_req_params *params)
{
	struct gs1500m_data *data = dev->data;

	if (!params || !params->ssid || params->ssid_length == 0U) {
		return -EINVAL;
	}
	if (!data->initialized) {
		return -EAGAIN;
	}
	if (data->connecting) {
		return -EINPROGRESS;
	}

	memset(data->ssid, 0, sizeof(data->ssid));
	memcpy(data->ssid, params->ssid,
	       MIN(params->ssid_length, sizeof(data->ssid) - 1U));

	memset(data->psk, 0, sizeof(data->psk));
	if (params->psk && params->psk_length > 0U) {
		memcpy(data->psk, params->psk,
		       MIN(params->psk_length, sizeof(data->psk) - 1U));
	}

	data->conn = *params;
	k_work_submit_to_queue(&data->workq, &data->connect_work);
	return 0;
}

static int gs_mgmt_disconnect(const struct device *dev)
{
	struct gs1500m_data *data = dev->data;

	if (!data->initialized) {
		return -EAGAIN;
	}
	k_work_submit_to_queue(&data->workq, &data->disconnect_work);
	return 0;
}

static int gs_mgmt_iface_status(const struct device *dev,
				struct wifi_iface_status *status)
{
	struct gs1500m_data *data = dev->data;
	gs_wifi_status_t st;
	const gs_wifi_module_info_t *mi;

	memset(status, 0, sizeof(*status));
	status->band = WIFI_FREQ_BAND_2_4_GHZ;
	status->iface_mode = WIFI_MODE_INFRA;
	status->link_mode = WIFI_LINK_MODE_UNKNOWN;
	status->security = WIFI_SECURITY_TYPE_UNKNOWN;
	status->mfp = WIFI_MFP_DISABLE;

	if (!data->initialized || !net_if_is_carrier_ok(data->iface)) {
		status->state = WIFI_STATE_INTERFACE_DISABLED;
		return 0;
	}

	k_mutex_lock(&data->lock, K_FOREVER);
	memset(&st, 0, sizeof(st));
	(void)gs_wifi_get_status(&st);
	mi = gs_wifi_last_module_info();
	k_mutex_unlock(&data->lock);

	if (data->connecting) {
		status->state = WIFI_STATE_ASSOCIATING;
	} else if (data->connected || st.associated) {
		status->state = WIFI_STATE_COMPLETED;
	} else {
		status->state = WIFI_STATE_DISCONNECTED;
	}

	if (st.ssid[0]) {
		strncpy(status->ssid, st.ssid, sizeof(status->ssid) - 1U);
	} else {
		strncpy(status->ssid, data->ssid, sizeof(status->ssid) - 1U);
	}
	status->ssid_len = strlen(status->ssid);

	if (mi && mi->mac[0]) {
		unsigned a, b, c, d, e, f;

		if (sscanf(mi->mac, "%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f) ==
		    6) {
			status->bssid[0] = (char)a;
			status->bssid[1] = (char)b;
			status->bssid[2] = (char)c;
			status->bssid[3] = (char)d;
			status->bssid[4] = (char)e;
			status->bssid[5] = (char)f;
		}
	}

	if (data->psk[0]) {
		status->security = WIFI_SECURITY_TYPE_PSK;
	} else {
		status->security = WIFI_SECURITY_TYPE_NONE;
	}

	return 0;
}

static void gs_iface_init(struct net_if *iface)
{
	struct gs1500m_data *data = net_if_get_device(iface)->data;

	data->iface = iface;
	(void)gs1500m_offload_init(iface);
	net_if_dormant_on(iface);
}

static enum offloaded_net_if_types gs_offload_get_type(void)
{
	return L2_OFFLOADED_NET_IF_TYPE_WIFI;
}

static const struct wifi_mgmt_ops gs_mgmt_ops = {
	.connect = gs_mgmt_connect,
	.disconnect = gs_mgmt_disconnect,
	.iface_status = gs_mgmt_iface_status,
};

static const struct net_wifi_mgmt_offload gs_api = {
	.wifi_iface.iface_api.init = gs_iface_init,
	.wifi_iface.get_type = gs_offload_get_type,
	.wifi_mgmt_api = &gs_mgmt_ops,
};

static int gs_init(const struct device *dev)
{
	struct gs1500m_data *data = dev->data;
	const struct gs1500m_config *cfg = dev->config;

	/* Platform bring-up uses board wifi-* GPIO aliases; DT config retained
	 * for binding completeness / future per-instance pin wiring.
	 */
	ARG_UNUSED(cfg);

	memset(data, 0, sizeof(*data));
	k_mutex_init(&data->lock);

	k_work_queue_start(&data->workq, gs_workq_stack,
			   K_KERNEL_STACK_SIZEOF(gs_workq_stack),
			   K_PRIO_COOP(CONFIG_WIFI_GS1500M_AT_WORKQ_PRIORITY),
			   NULL);
	k_thread_name_set(&data->workq.thread, "gs1500m_wq");

	k_work_init(&data->init_work, gs_init_work_fn);
	k_work_init(&data->connect_work, gs_connect_work_fn);
	k_work_init(&data->disconnect_work, gs_disconnect_work_fn);

	data->rx_stack = gs_rx_stack;
	k_thread_create(&data->rx_thread, gs_rx_stack,
			K_KERNEL_STACK_SIZEOF(gs_rx_stack), gs_rx_thread_fn, data,
			NULL, NULL, K_PRIO_COOP(CONFIG_WIFI_GS1500M_AT_RX_PRIORITY),
			0, K_NO_WAIT);
	k_thread_name_set(&data->rx_thread, "gs1500m_rx");

	k_work_submit_to_queue(&data->workq, &data->init_work);
	LOG_INF("GS1500M offload driver queued bring-up");
	return 0;
}

NET_DEVICE_DT_INST_OFFLOAD_DEFINE(0, gs_init, NULL, &gs_data, &gs_config,
				  CONFIG_WIFI_INIT_PRIORITY, &gs_api, GS1500M_MTU);

CONNECTIVITY_WIFI_MGMT_BIND(Z_DEVICE_DT_DEV_ID(DT_DRV_INST(0)));
