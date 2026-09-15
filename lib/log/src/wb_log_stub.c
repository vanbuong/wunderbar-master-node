/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Capture backend for Unity / ztest.
 */

#include "wb_log.h"

#include <string.h>

#ifndef WB_LOG_STUB_CAPACITY
#define WB_LOG_STUB_CAPACITY 1024
#endif

static char s_buf[WB_LOG_STUB_CAPACITY];
static size_t s_len;

static int stub_write(const void *data, size_t len, void *ctx)
{
	size_t space;
	size_t copy;

	(void)ctx;
	if ((data == NULL) || (len == 0U)) {
		return 0;
	}

	space = WB_LOG_STUB_CAPACITY - 1U - s_len;
	copy = (len < space) ? len : space;
	if (copy > 0U) {
		memcpy(s_buf + s_len, data, copy);
		s_len += copy;
		s_buf[s_len] = '\0';
	}
	return (int)copy;
}

static const wb_log_backend_t s_stub_backend = {
	.write = stub_write,
	.ctx = NULL,
};

void wb_log_stub_reset(void)
{
	s_len = 0;
	s_buf[0] = '\0';
}

const wb_log_backend_t *wb_log_stub_backend(void)
{
	return &s_stub_backend;
}

const char *wb_log_stub_buffer(void)
{
	return s_buf;
}

size_t wb_log_stub_length(void)
{
	return s_len;
}

bool wb_log_stub_contains(const char *needle)
{
	if ((needle == NULL) || (needle[0] == '\0')) {
		return false;
	}
	return strstr(s_buf, needle) != NULL;
}
