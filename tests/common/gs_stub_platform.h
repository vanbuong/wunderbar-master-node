/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Host stub platform for gs1500m unit tests (no module / UART).
 */

#ifndef GS_STUB_PLATFORM_H
#define GS_STUB_PLATFORM_H

#include "gs1500m/platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GS_STUB_TX_MAX 4096U
#define GS_STUB_RX_MAX 4096U

typedef struct {
	uint8_t tx[GS_STUB_TX_MAX];
	size_t tx_len;
	uint8_t rx[GS_STUB_RX_MAX];
	size_t rx_len;
	size_t rx_pos;
	uint32_t now_ms;
	bool auto_ok;           /* queue OK/CONNECT for AT lines written */
	unsigned reset_pulses;  /* count of assert→release cycles */
	bool last_intf_uart;
	bool last_pgm_assert;
} gs_stub_ctx_t;

void gs_stub_reset(gs_stub_ctx_t *ctx);
/** Install platform vtable bound to ctx (auto_ok enabled by default). */
void gs_stub_install(gs_stub_ctx_t *ctx, gs_platform_t *out);

void gs_stub_rx_push(gs_stub_ctx_t *ctx, const void *data, size_t len);
void gs_stub_rx_push_str(gs_stub_ctx_t *ctx, const char *s);

/** True if captured TX contains needle (binary-safe via memmem-like scan). */
bool gs_stub_tx_contains(const gs_stub_ctx_t *ctx, const char *needle);

/** NUL-terminated copy of TX for string asserts (truncated). */
const char *gs_stub_tx_cstr(gs_stub_ctx_t *ctx, char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* GS_STUB_PLATFORM_H */
