/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Unity host tests for wb_time.
 */

#include "unity.h"
#include "wb_time.h"

#include <string.h>

static uint32_t s_ms;

static uint32_t fake_millis(void *ctx)
{
	(void)ctx;
	return s_ms;
}

void setUp(void)
{
	s_ms = 0;
	wb_time_init(fake_millis, NULL);
	wb_time_set_unix(0);
}

void tearDown(void)
{
}

void test_unsynced_log_prefix_is_boot_relative(void)
{
	char buf[40];

	s_ms = 12345U;
	TEST_ASSERT_FALSE(wb_time_is_synced());
	TEST_ASSERT_EQUAL_UINT(0, wb_time_get_unix());
	TEST_ASSERT_TRUE(wb_time_format_log(buf, sizeof(buf)) > 0);
	TEST_ASSERT_EQUAL_STRING("T+000012.345", buf);
}

void test_set_unix_syncs_wall_clock(void)
{
	char buf[40];
	char utc[32];

	/* 2025-09-16 16:00:00 UTC */
	s_ms = 1000U;
	wb_time_set_unix(1758038400U);
	TEST_ASSERT_TRUE(wb_time_is_synced());
	TEST_ASSERT_EQUAL_UINT(1758038400U, wb_time_get_unix());

	s_ms = 1000U + 1500U; /* +1.5s */
	TEST_ASSERT_EQUAL_UINT(1758038401U, wb_time_get_unix());
	TEST_ASSERT_TRUE(wb_time_format_log(buf, sizeof(buf)) > 0);
	TEST_ASSERT_TRUE(strstr(buf, "2025-09-16 16:00:01.500") != NULL);

	TEST_ASSERT_TRUE(wb_time_format_utc(1758038400U, utc, sizeof(utc)) > 0);
	TEST_ASSERT_EQUAL_STRING("2025-09-16 16:00:00", utc);
}

void test_millis_wrap_still_advances(void)
{
	s_ms = 0xFFFFFFF0U;
	wb_time_set_unix(1000U);
	s_ms = 0x10U; /* wrapped +32 ms */
	TEST_ASSERT_EQUAL_UINT(1000U, wb_time_get_unix());
	TEST_ASSERT_EQUAL_UINT64(1000000ULL + 32ULL, wb_time_get_unix_ms());
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_unsynced_log_prefix_is_boot_relative);
	RUN_TEST(test_set_unix_syncs_wall_clock);
	RUN_TEST(test_millis_wrap_still_advances);
	return UNITY_END();
}
