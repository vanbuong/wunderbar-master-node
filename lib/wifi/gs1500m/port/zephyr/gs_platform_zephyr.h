/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef GS_PLATFORM_ZEPHYR_H
#define GS_PLATFORM_ZEPHYR_H

#include "gs1500m/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bind to DTS uart0 + wifi-* GPIO aliases and install platform. */
int gs_platform_zephyr_init(gs_platform_t *out);

void gs_platform_zephyr_rx_poll(uint32_t block_ms);

#ifdef __cplusplus
}
#endif

#endif /* GS_PLATFORM_ZEPHYR_H */
