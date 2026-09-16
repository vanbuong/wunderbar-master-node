/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Unity unit tests for wb_log (host).
 */

#include "unity.h"
#include "wb_log.h"
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
	wb_log_stub_reset();
	wb_log_init(wb_log_stub_backend(), WB_LOG_INFO);
}

void tearDown(void)
{
}

void test_info_emitted_at_info_level(void)
{
	WB_LOGI("hello %d", 42);
	TEST_ASSERT_TRUE(wb_log_stub_contains("[I] hello 42"));
	TEST_ASSERT_TRUE(wb_log_stub_contains("[T+"));
	TEST_ASSERT_TRUE(wb_log_stub_contains("\n"));
}

void test_debug_filtered_when_min_is_info(void)
{
	WB_LOGD("secret");
	TEST_ASSERT_EQUAL_UINT(0, wb_log_stub_length());
	TEST_ASSERT_FALSE(wb_log_stub_contains("secret"));
}

void test_debug_emitted_when_level_raised(void)
{
	wb_log_set_level(WB_LOG_DBG);
	WB_LOGD("detail");
	TEST_ASSERT_TRUE(wb_log_stub_contains("[D] detail"));
}

void test_error_and_warn_tags(void)
{
	WB_LOGE("boom");
	WB_LOGW("careful");
	TEST_ASSERT_TRUE(wb_log_stub_contains("[E] boom"));
	TEST_ASSERT_TRUE(wb_log_stub_contains("[W] careful"));
}

void test_level_getter(void)
{
	TEST_ASSERT_EQUAL_INT(WB_LOG_INFO, wb_log_get_level());
	wb_log_set_level(WB_LOG_ERR);
	TEST_ASSERT_EQUAL_INT(WB_LOG_ERR, wb_log_get_level());
	WB_LOGI("hidden");
	TEST_ASSERT_EQUAL_UINT(0, wb_log_stub_length());
	WB_LOGE("shown");
	TEST_ASSERT_TRUE(wb_log_stub_contains("[E] shown"));
}

void test_null_backend_is_safe(void)
{
	wb_log_init(NULL, WB_LOG_DBG);
	TEST_ASSERT_EQUAL_INT(0, wb_log_write(WB_LOG_INFO, "noop"));
}

void test_log_uses_ntp_wall_clock_after_sync(void)
{
	s_ms = 0;
	wb_time_set_unix(1758038400U); /* 2025-09-16 16:00:00 */
	s_ms = 250U;
	wb_log_stub_reset();
	WB_LOGI("synced");
	TEST_ASSERT_TRUE(wb_log_stub_contains("2025-09-16 16:00:00.250"));
	TEST_ASSERT_TRUE(wb_log_stub_contains("[I] synced"));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_info_emitted_at_info_level);
	RUN_TEST(test_debug_filtered_when_min_is_info);
	RUN_TEST(test_debug_emitted_when_level_raised);
	RUN_TEST(test_error_and_warn_tags);
	RUN_TEST(test_level_getter);
	RUN_TEST(test_null_backend_is_safe);
	RUN_TEST(test_log_uses_ntp_wall_clock_after_sync);
	return UNITY_END();
}
