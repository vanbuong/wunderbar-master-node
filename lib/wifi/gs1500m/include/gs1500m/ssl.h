/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef GS1500M_SSL_H
#define GS1500M_SSL_H

#include "gs1500m/at.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Add DER cert: AT+TCERTADD then ESC W + payload. */
gs_msg_id_t gs_ssl_cert_add(const char *name, bool to_flash,
			    const uint8_t *der, size_t len);
gs_msg_id_t gs_ssl_cert_delete(const char *name);

/**
 * Open TLS on an existing TCP CID (AT+SSLOPEN).
 * cacert may be "" / NULL to skip server verification.
 * client_cert/client_key optional (leave NULL if unused).
 */
gs_msg_id_t gs_ssl_open(uint8_t cid, const char *cacert,
			const char *client_cert, const char *client_key);
gs_msg_id_t gs_ssl_close(uint8_t cid);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_SSL_H */
