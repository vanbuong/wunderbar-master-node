/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Portable WunderBar log module — OS-agnostic formatting + level filter.
 * Output goes through a pluggable backend (stdio, stub, USB, RTT, …).
 */

#ifndef WB_LOG_H
#define WB_LOG_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	WB_LOG_ERR = 0,
	WB_LOG_WARN,
	WB_LOG_INFO,
	WB_LOG_DBG,
} wb_log_level_t;

typedef int (*wb_log_writer_t)(const void *data, size_t len, void *ctx);

typedef struct {
	wb_log_writer_t write;
	void *ctx;
} wb_log_backend_t;

/** Initialize logging. Safe to call again to swap backend / level. */
void wb_log_init(const wb_log_backend_t *backend, wb_log_level_t min_level);

/** Change the minimum level that is emitted. */
void wb_log_set_level(wb_log_level_t min_level);

/** Current minimum level. */
wb_log_level_t wb_log_get_level(void);

/**
 * Format and emit one log line: "<tag> message\\n".
 * Returns bytes written to the backend, or 0 if filtered / no backend.
 */
int wb_log_write(wb_log_level_t level, const char *fmt, ...);

#define WB_LOGE(...) wb_log_write(WB_LOG_ERR, __VA_ARGS__)
#define WB_LOGW(...) wb_log_write(WB_LOG_WARN, __VA_ARGS__)
#define WB_LOGI(...) wb_log_write(WB_LOG_INFO, __VA_ARGS__)
#define WB_LOGD(...) wb_log_write(WB_LOG_DBG, __VA_ARGS__)

/* ---- stdio backend (uses fwrite(stdout); FreeRTOS/_write or Zephyr console) ---- */
int wb_log_stdio_write(const void *data, size_t len, void *ctx);
const wb_log_backend_t *wb_log_stdio_backend(void);

/* ---- stub backend (unit tests capture output) ---- */
void wb_log_stub_reset(void);
const wb_log_backend_t *wb_log_stub_backend(void);
const char *wb_log_stub_buffer(void);
size_t wb_log_stub_length(void);
bool wb_log_stub_contains(const char *needle);

#ifdef __cplusplus
}
#endif

#endif /* WB_LOG_H */
