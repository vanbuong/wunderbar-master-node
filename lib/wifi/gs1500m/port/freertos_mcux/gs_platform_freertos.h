/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOS + MCUXpresso SDK platform for GS1500M (UART0 + control GPIOs).
 */

#ifndef GS_PLATFORM_FREERTOS_H
#define GS_PLATFORM_FREERTOS_H

#include "gs1500m/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Init UART0 @ 115200 and WiFi control pins; fill and install platform. */
int gs_platform_freertos_init(gs_platform_t *out);

/** Optional RX pump from a FreeRTOS task — reads UART into gs_at_process_byte. */
void gs_platform_freertos_rx_poll(uint32_t block_ms);

#ifdef __cplusplus
}
#endif

#endif /* GS_PLATFORM_FREERTOS_H */
