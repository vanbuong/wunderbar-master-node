/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_log.h"

#include <stdio.h>

int wb_log_stdio_write(const void *data, size_t len, void *ctx)
{
	size_t n;

	(void)ctx;
	if ((data == NULL) || (len == 0U)) {
		return 0;
	}

	n = fwrite(data, 1, len, stdout);
	(void)fflush(stdout);
	return (int)n;
}

static const wb_log_backend_t s_stdio_backend = {
	.write = wb_log_stdio_write,
	.ctx = NULL,
};

const wb_log_backend_t *wb_log_stdio_backend(void)
{
	return &s_stdio_backend;
}
