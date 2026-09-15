/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * ztest suite for GS1500M AT line/ESC parser.
 */

#include <zephyr/ztest.h>
#include "gs1500m/at.h"

#include <string.h>

static uint8_t s_rx_cid;
static uint8_t s_rx_buf[64];
static size_t s_rx_len;

static void on_data(uint8_t cid, gs_esc_kind_t kind, const uint8_t *data,
		    size_t len, void *user)
{
	(void)kind;
	(void)user;
	s_rx_cid = cid;
	if (s_rx_len + len > sizeof(s_rx_buf)) {
		len = sizeof(s_rx_buf) - s_rx_len;
	}
	memcpy(s_rx_buf + s_rx_len, data, len);
	s_rx_len += len;
}

static void *gs_at_setup(void)
{
	gs_at_callbacks_t cbs = { .on_data = on_data };
	s_rx_len = 0;
	gs_at_init(&cbs);
	return NULL;
}

static void gs_at_before(void *fixture)
{
	(void)fixture;
	s_rx_len = 0;
	s_rx_cid = GS_AT_INVALID_CID;
	memset(s_rx_buf, 0, sizeof(s_rx_buf));
	gs_at_callbacks_t cbs = { .on_data = on_data };
	gs_at_init(&cbs);
}

ZTEST_SUITE(gs1500m_at_tests, NULL, gs_at_setup, gs_at_before, NULL, NULL);

ZTEST(gs1500m_at_tests, test_classify_ok)
{
	zassert_equal(gs_at_classify_line("OK"), GS_MSG_OK);
	zassert_equal(gs_at_classify_line("ERROR"), GS_MSG_ERROR);
}

ZTEST(gs1500m_at_tests, test_bulk_frame)
{
	uint8_t frame[] = {
		0x1B, 'Z', '1', '0', '0', '0', '4', 't', 'e', 's', 't'
	};
	gs_msg_id_t id = gs_at_process_chunk(frame, sizeof(frame));
	zassert_equal(id, GS_MSG_BULK_DATA);
	zassert_equal(s_rx_cid, 1);
	zassert_equal(s_rx_len, 4);
	zassert_mem_equal(s_rx_buf, "test", 4);
}

ZTEST(gs1500m_at_tests, test_4digit)
{
	char d[5];
	gs_at_u32_to_4digit(7, d);
	zassert_str_equal(d, "0007");
}

ZTEST(gs1500m_at_tests, test_line_ok)
{
	const char *s = "OK\r\n";
	zassert_equal(gs_at_process_chunk((const uint8_t *)s, 4), GS_MSG_OK);
}
