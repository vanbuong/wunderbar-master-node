/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs1500m/platform.h"

static const gs_platform_t *s_platform;

void gs_platform_set(const gs_platform_t *platform)
{
	s_platform = platform;
}

const gs_platform_t *gs_platform_get(void)
{
	return s_platform;
}
