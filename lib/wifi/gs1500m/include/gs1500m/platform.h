/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Portable HAL vtable for GS1500M UART AT + control GPIOs.
 */

#ifndef GS1500M_PLATFORM_H
#define GS1500M_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gs_platform {
	/** Write bytes to the module UART. Returns bytes written or <0 on error. */
	int (*uart_write)(const uint8_t *data, size_t len, void *ctx);
	/**
	 * Read up to max_len bytes. If block_ms == 0, non-blocking.
	 * Returns bytes read (0 = none / timeout), or <0 on error.
	 */
	int (*uart_read)(uint8_t *data, size_t max_len, uint32_t block_ms, void *ctx);
	/** Discard pending RX bytes. */
	void (*uart_flush)(void *ctx);
	/** Milliseconds since boot (monotonic). */
	uint32_t (*millis)(void *ctx);
	/** Busy or OS delay. */
	void (*delay_ms)(uint32_t ms, void *ctx);
	/** Drive WIFI_!RESET (true = assert / hold in reset). */
	void (*reset_set)(bool assert_reset, void *ctx);
	/** Drive WIFI_INTF_SEL to UART mode (uses GS_INTF_SEL_UART_LEVEL). */
	void (*intf_sel_uart)(void *ctx);
	/** Drive WIFI_PGM idle (normal boot) or assert for programming. */
	void (*pgm_set)(bool assert_pgm, void *ctx);
	void *ctx;
} gs_platform_t;

/** Install the active platform (required before gs_at / gs_wifi calls). */
void gs_platform_set(const gs_platform_t *platform);

/** Current platform, or NULL if unset. */
const gs_platform_t *gs_platform_get(void);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_PLATFORM_H */
