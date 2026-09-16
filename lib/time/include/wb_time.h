/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Portable system wall-clock time. Monotonic millis come from a platform
 * callback; wall time is set from NTP (or any Unix-seconds source).
 */

#ifndef WB_TIME_H
#define WB_TIME_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Milliseconds since an arbitrary boot epoch (wraps every ~49 days). */
typedef uint32_t (*wb_time_millis_fn)(void *ctx);

/**
 * Bind the monotonic tick source. Safe to call again.
 * Until set_unix(), log timestamps use boot-relative "T+sss.mmm".
 */
void wb_time_init(wb_time_millis_fn millis, void *ctx);

/** True after a successful wb_time_set_unix(). */
bool wb_time_is_synced(void);

/**
 * Anchor wall-clock Unix seconds (UTC) at the current monotonic tick.
 * Called after NTP sync.
 */
void wb_time_set_unix(uint32_t unix_sec);

/** Current UTC Unix seconds, or 0 if not synced. */
uint32_t wb_time_get_unix(void);

/** Current UTC Unix time in milliseconds, or boot uptime ms if unsynced. */
uint64_t wb_time_get_unix_ms(void);

/**
 * Format a log timestamp prefix (no brackets).
 * Synced:   "YYYY-MM-DD HH:MM:SS.mmm"
 * Unsynced: "T+SSSSSS.mmm" (boot-relative)
 * Returns bytes written (excluding NUL), or 0 on error.
 */
int wb_time_format_log(char *buf, size_t buflen);

/**
 * Format UTC calendar time from Unix seconds into
 * "YYYY-MM-DD HH:MM:SS" (buflen >= 20).
 */
int wb_time_format_utc(uint32_t unix_sec, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* WB_TIME_H */
