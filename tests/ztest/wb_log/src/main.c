/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * ztest suite for wb_log.
 */

#include <zephyr/ztest.h>
#include "wb_log.h"
#include "wb_time.h"

static uint32_t s_ms;

static uint32_t fake_millis(void *ctx)
{
	(void)ctx;
	return s_ms;
}

static void *wb_log_setup(void)
{
	s_ms = 0;
	wb_time_init(fake_millis, NULL);
	wb_time_set_unix(0);
	wb_log_stub_reset();
	wb_log_init(wb_log_stub_backend(), WB_LOG_INFO);
	return NULL;
}

static void wb_log_before(void *fixture)
{
	(void)fixture;
	s_ms = 0;
	wb_time_init(fake_millis, NULL);
	wb_time_set_unix(0);
	wb_log_stub_reset();
	wb_log_set_level(WB_LOG_INFO);
}

ZTEST_SUITE(wb_log_tests, NULL, wb_log_setup, wb_log_before, NULL, NULL);

ZTEST(wb_log_tests, test_info_emitted)
{
	WB_LOGI("hello %d", 7);
	zassert_true(wb_log_stub_contains("[I] hello 7"), "missing info line");
	zassert_true(wb_log_stub_contains("[T+"), "missing time prefix");
}

ZTEST(wb_log_tests, test_debug_filtered)
{
	WB_LOGD("nope");
	zassert_equal(wb_log_stub_length(), 0, "debug should be filtered");
}

ZTEST(wb_log_tests, test_debug_when_enabled)
{
	wb_log_set_level(WB_LOG_DBG);
	WB_LOGD("yes");
	zassert_true(wb_log_stub_contains("[D] yes"), "missing debug line");
}

ZTEST(wb_log_tests, test_error_warn_tags)
{
	WB_LOGE("err");
	WB_LOGW("warn");
	zassert_true(wb_log_stub_contains("[E] err"));
	zassert_true(wb_log_stub_contains("[W] warn"));
}

ZTEST(wb_log_tests, test_level_filter_error_only)
{
	wb_log_set_level(WB_LOG_ERR);
	WB_LOGI("skip");
	zassert_equal(wb_log_stub_length(), 0);
	WB_LOGE("keep");
	zassert_true(wb_log_stub_contains("[E] keep"));
}

ZTEST(wb_log_tests, test_wall_clock_after_ntp)
{
	wb_time_set_unix(1758038400U);
	s_ms = 100U;
	wb_log_stub_reset();
	WB_LOGI("wall");
	zassert_true(wb_log_stub_contains("2025-09-16 16:00:00.100"));
}
