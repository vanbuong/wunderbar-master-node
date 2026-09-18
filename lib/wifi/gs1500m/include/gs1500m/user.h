/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Product-level state machine (legacy GS_Main_StateMachine role):
 * init → join → optional HTTP time / CA → TLS MQTT pipe, or Limited AP onboarding.
 */

#ifndef GS1500M_USER_H
#define GS1500M_USER_H

#include "gs1500m/at.h"
#include "gs1500m/mqtt_pipe.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	GS_USER_IDLE = 0,
	GS_USER_INIT,
	GS_USER_JOIN,
	GS_USER_HTTP_TIME,
	GS_USER_LOAD_CA,
	GS_USER_MQTT,
	GS_USER_LIMITED_AP,
	GS_USER_READY,
	GS_USER_ERROR,
} gs_user_state_t;

typedef struct {
	const char *ssid;
	const char *psk;
	const char *mqtt_host;
	uint16_t mqtt_port;
	bool mqtt_tls;
	const char *cacert_name;
	const uint8_t *cacert_der;
	size_t cacert_der_len;
	bool use_limited_ap_on_fail;
	const char *lap_ssid;
	const char *lap_ip;
} gs_user_config_t;

typedef struct {
	gs_user_state_t state;
	gs_msg_id_t last_msg;
	gs_mqtt_pipe_t mqtt;
	gs_user_config_t cfg;
} gs_user_t;

void gs_user_init(gs_user_t *u, const gs_user_config_t *cfg);

/** Advance one step; call periodically from an app task. */
gs_user_state_t gs_user_poll(gs_user_t *u);

gs_user_state_t gs_user_state(const gs_user_t *u);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_USER_H */
