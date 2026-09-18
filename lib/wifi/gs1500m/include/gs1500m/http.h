/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef GS1500M_HTTP_H
#define GS1500M_HTTP_H

#include "gs1500m/at.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

gs_msg_id_t gs_http_open(const char *host, uint16_t port, bool tls,
			 const char *cacert_or_null, uint8_t *cid_out);

/**
 * AT+HTTPSEND=<cid>,<type>,<timeout>,<path>[,content-length]
 * followed by ESC H payload when body_len > 0.
 * type: 1=GET, 2=HEAD, 3=POST, …
 */
gs_msg_id_t gs_http_send(uint8_t cid, int method, uint32_t timeout_s,
			 const char *path, const uint8_t *body, size_t body_len);

gs_msg_id_t gs_http_close(uint8_t cid);
gs_msg_id_t gs_http_conf(int param, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_HTTP_H */
