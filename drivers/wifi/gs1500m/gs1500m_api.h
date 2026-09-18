/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Public helpers for apps using the GS1500M Zephyr offload driver.
 */

#ifndef ZEPHYR_DRIVERS_WIFI_GS1500M_API_H_
#define ZEPHYR_DRIVERS_WIFI_GS1500M_API_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve @p host via module AT+DNSLOOKUP (serialized with the driver lock).
 * On success writes a dotted-quad into @p ip.
 *
 * @return 0 on success, negative errno otherwise.
 */
int gs1500m_dns_lookup(const char *host, char *ip, size_t ip_len);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_WIFI_GS1500M_API_H_ */
