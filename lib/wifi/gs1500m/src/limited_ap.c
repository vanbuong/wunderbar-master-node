/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/limited_ap.h"
#include "gs1500m/wifi.h"

#include <stdbool.h>

gs_msg_id_t gs_lap_dhcp_server(bool on)
{
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+DHCPSRVR=%u\r\n",
			       on ? 1U : 0U);
}

gs_msg_id_t gs_lap_webprov(const char *username, const char *password)
{
	if (!username || !password) {
		return GS_MSG_INVALID_INPUT;
	}
	return gs_at_send_cmdf(GS_AT_DEFAULT_CMD_TIMEOUT_MS, "AT+WEBPROV=%s,%s\r\n",
			       username, password);
}

gs_msg_id_t gs_lap_start(const char *ssid, const char *channel,
			 const char *ip, const char *mask, const char *gw)
{
	gs_msg_id_t id;
	const char *use_ip = ip ? ip : "192.168.240.1";
	const char *use_mask = mask ? mask : "255.255.255.0";
	const char *use_gw = gw ? gw : use_ip;
	const char *use_ch = channel ? channel : "6";

	if (!ssid) {
		return GS_MSG_INVALID_INPUT;
	}

	id = gs_wifi_dhcp_client(false);
	if (id != GS_MSG_OK) {
		return id;
	}
	id = gs_wifi_set_ip(use_ip, use_mask, use_gw);
	if (id != GS_MSG_OK) {
		return id;
	}
	id = gs_wifi_set_mode(GS_WIFI_MODE_LIMITED_AP);
	if (id != GS_MSG_OK) {
		return id;
	}
	id = gs_wifi_set_security(GS_WIFI_SEC_OPEN);
	if (id != GS_MSG_OK) {
		return id;
	}
	id = gs_wifi_join(ssid, NULL, use_ch);
	if (id != GS_MSG_OK) {
		return id;
	}
	return gs_lap_dhcp_server(true);
}

gs_msg_id_t gs_lap_stop(void)
{
	(void)gs_lap_dhcp_server(false);
	return gs_wifi_disconnect();
}
