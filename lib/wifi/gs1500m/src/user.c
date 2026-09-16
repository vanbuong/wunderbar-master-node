/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/user.h"
#include "gs1500m/wifi.h"
#include "gs1500m/ssl.h"
#include "gs1500m/limited_ap.h"
#include "gs1500m/http.h"
#include "gs1500m/platform.h"
#include "wb_time.h"

#include <string.h>

static void delay_join_retry(void)
{
	const gs_platform_t *p = gs_platform_get();

	if (p && p->delay_ms) {
		p->delay_ms(500U, p->ctx);
	}
}

void gs_user_init(gs_user_t *u, const gs_user_config_t *cfg)
{
	if (!u) {
		return;
	}
	memset(u, 0, sizeof(*u));
	u->state = GS_USER_IDLE;
	if (cfg) {
		u->cfg = *cfg;
	}
}

gs_user_state_t gs_user_state(const gs_user_t *u)
{
	return u ? u->state : GS_USER_ERROR;
}

gs_user_state_t gs_user_poll(gs_user_t *u)
{
	gs_msg_id_t id;

	if (!u) {
		return GS_USER_ERROR;
	}

	switch (u->state) {
	case GS_USER_IDLE:
		u->state = GS_USER_INIT;
		break;

	case GS_USER_INIT:
		id = gs_wifi_init(8000U);
		u->last_msg = id;
		if (id == GS_MSG_OK) {
			u->state = GS_USER_JOIN;
		} else {
			u->state = GS_USER_ERROR;
		}
		break;

	case GS_USER_JOIN:
		if (!u->cfg.ssid) {
			u->state = GS_USER_ERROR;
			break;
		}
		id = gs_wifi_join_wpa(u->cfg.ssid, u->cfg.psk);
		/*
		 * Module sometimes warm-boots during the first associate. Re-init
		 * the UART link once and retry.
		 */
		if (id == GS_MSG_APP_RESET || id == GS_MSG_TIMEOUT) {
			gs_at_flush();
			delay_join_retry();
			if (gs_wifi_init(8000U) == GS_MSG_OK) {
				id = gs_wifi_join_wpa(u->cfg.ssid, u->cfg.psk);
			}
		}
		u->last_msg = id;
		if (id == GS_MSG_OK) {
			(void)gs_wifi_bulk_data(true);
			(void)gs_wifi_query_module_info(NULL);
			if (!gs_wifi_last_ip()[0]) {
				(void)gs_wifi_get_status(NULL);
			}
			u->state = GS_USER_HTTP_TIME;
		} else if (u->cfg.use_limited_ap_on_fail) {
			u->state = GS_USER_LIMITED_AP;
		} else {
			u->state = GS_USER_ERROR;
		}
		break;

	case GS_USER_HTTP_TIME: {
		uint32_t unix_sec = 0U;
		/* SNTP over UDP, then SETTIME / GETTIME on the module. */
		id = gs_wifi_ntp_sync(&unix_sec, NULL, 0U);
		u->last_msg = id;
		if (id == GS_MSG_OK) {
			if (unix_sec == 0U) {
				unix_sec = gs_wifi_last_unix_time();
			}
			if (unix_sec != 0U) {
				wb_time_set_unix(unix_sec);
			}
		}
		/* Time sync is best-effort; continue bring-up either way. */
		u->state = GS_USER_LOAD_CA;
		break;
	}

	case GS_USER_LOAD_CA:
		if (u->cfg.cacert_der && u->cfg.cacert_der_len > 0U &&
		    u->cfg.cacert_name) {
			id = gs_ssl_cert_add(u->cfg.cacert_name, true,
					     u->cfg.cacert_der,
					     u->cfg.cacert_der_len);
			u->last_msg = id;
			if (id != GS_MSG_OK) {
				u->state = GS_USER_ERROR;
				break;
			}
		}
		u->state = (u->cfg.mqtt_host) ? GS_USER_MQTT : GS_USER_READY;
		break;

	case GS_USER_MQTT:
		id = gs_mqtt_pipe_open(&u->mqtt, u->cfg.mqtt_host,
				       u->cfg.mqtt_port ? u->cfg.mqtt_port : 8883U,
				       u->cfg.mqtt_tls, u->cfg.cacert_name);
		u->last_msg = id;
		u->state = (id == GS_MSG_OK) ? GS_USER_READY : GS_USER_ERROR;
		break;

	case GS_USER_LIMITED_AP:
		id = gs_lap_start(u->cfg.lap_ssid ? u->cfg.lap_ssid : "WunderBar-Setup",
				  "6",
				  u->cfg.lap_ip ? u->cfg.lap_ip : "192.168.240.1",
				  "255.255.255.0", NULL);
		u->last_msg = id;
		u->state = (id == GS_MSG_OK) ? GS_USER_READY : GS_USER_ERROR;
		break;

	case GS_USER_READY:
	case GS_USER_ERROR:
	default:
		break;
	}

	return u->state;
}
