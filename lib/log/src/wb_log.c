/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_log.h"
#include "wb_time.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifndef WB_LOG_LINE_MAX
#define WB_LOG_LINE_MAX 192
#endif

static const wb_log_backend_t *s_backend;
static wb_log_level_t s_min_level = WB_LOG_INFO;

static const char *level_tag(wb_log_level_t level)
{
	switch (level) {
	case WB_LOG_ERR:
		return "E";
	case WB_LOG_WARN:
		return "W";
	case WB_LOG_INFO:
		return "I";
	case WB_LOG_DBG:
		return "D";
	default:
		return "?";
	}
}

void wb_log_init(const wb_log_backend_t *backend, wb_log_level_t min_level)
{
	s_backend = backend;
	s_min_level = min_level;
}

void wb_log_set_level(wb_log_level_t min_level)
{
	s_min_level = min_level;
}

wb_log_level_t wb_log_get_level(void)
{
	return s_min_level;
}

int wb_log_write(wb_log_level_t level, const char *fmt, ...)
{
	char line[WB_LOG_LINE_MAX];
	char ts[32];
	int ts_len;
	int prefix_len;
	int body_len;
	int total;
	va_list ap;

	if ((s_backend == NULL) || (s_backend->write == NULL)) {
		return 0;
	}
	if (level > s_min_level) {
		return 0;
	}
	if (fmt == NULL) {
		return 0;
	}

	ts_len = wb_time_format_log(ts, sizeof(ts));
	if (ts_len > 0) {
		prefix_len = snprintf(line, sizeof(line), "[%s][%s] ", ts,
				      level_tag(level));
	} else {
		prefix_len = snprintf(line, sizeof(line), "[%s] ",
				      level_tag(level));
	}
	if (prefix_len < 0) {
		return 0;
	}
	if ((size_t)prefix_len >= sizeof(line)) {
		prefix_len = (int)sizeof(line) - 1;
	}

	va_start(ap, fmt);
	body_len = vsnprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len,
			     fmt, ap);
	va_end(ap);

	if (body_len < 0) {
		return 0;
	}

	total = prefix_len + body_len;
	if ((size_t)total >= sizeof(line)) {
		total = (int)sizeof(line) - 1;
		line[total] = '\0';
	}

	/* Ensure a trailing newline when there is room. */
	if ((total == 0) || (line[total - 1] != '\n')) {
		if ((size_t)total + 1 < sizeof(line)) {
			line[total++] = '\n';
			line[total] = '\0';
		}
	}

	return s_backend->write(line, (size_t)total, s_backend->ctx);
}
