/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Byte pipe over TLS TCP — legacy GS_TCP_mqtt role.
 * Application owns MQTT framing; this layer only moves bytes.
 */

#ifndef GS1500M_MQTT_PIPE_H
#define GS1500M_MQTT_PIPE_H

#include "gs1500m/at.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t cid;
	bool open;
	bool tls;
} gs_mqtt_pipe_t;

gs_msg_id_t gs_mqtt_pipe_open(gs_mqtt_pipe_t *pipe, const char *host,
			      uint16_t port, bool tls, const char *cacert);
gs_msg_id_t gs_mqtt_pipe_send(gs_mqtt_pipe_t *pipe, const uint8_t *data,
			      size_t len);
gs_msg_id_t gs_mqtt_pipe_close(gs_mqtt_pipe_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_MQTT_PIPE_H */
