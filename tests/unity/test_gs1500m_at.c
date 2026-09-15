/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Unity host tests for GS1500M AT line/ESC parser and helpers.
 */

#include "unity.h"
#include "gs1500m/at.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

static uint8_t s_rx_cid;
static gs_esc_kind_t s_rx_kind;
static uint8_t s_rx_buf[128];
static size_t s_rx_len;
static gs_msg_id_t s_last_line_id;
static char s_last_async[64];

static void on_data(uint8_t cid, gs_esc_kind_t kind, const uint8_t *data,
		    size_t len, void *user)
{
	(void)user;
	s_rx_cid = cid;
	s_rx_kind = kind;
	if (s_rx_len + len > sizeof(s_rx_buf)) {
		len = sizeof(s_rx_buf) - s_rx_len;
	}
	memcpy(s_rx_buf + s_rx_len, data, len);
	s_rx_len += len;
}

static void on_line(gs_msg_id_t id, const char *line, void *user)
{
	(void)user;
	s_last_line_id = id;
	strncpy(s_last_async, line ? line : "", sizeof(s_last_async) - 1U);
}

void setUp(void)
{
	gs_at_callbacks_t cbs = {
		.on_data = on_data,
		.on_line = on_line,
		.user = NULL,
	};
	s_rx_cid = GS_AT_INVALID_CID;
	s_rx_kind = GS_ESC_KIND_NONE;
	s_rx_len = 0;
	s_last_line_id = GS_MSG_NONE;
	s_last_async[0] = '\0';
	memset(s_rx_buf, 0, sizeof(s_rx_buf));
	gs_at_init(&cbs);
}

void tearDown(void)
{
}

void test_classify_ok_error(void)
{
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_at_classify_line("OK\r\n"));
	TEST_ASSERT_EQUAL_INT(GS_MSG_ERROR, gs_at_classify_line("ERROR\r\n"));
	TEST_ASSERT_EQUAL_INT(GS_MSG_INVALID_INPUT,
			      gs_at_classify_line("ERROR: INVALID INPUT"));
	TEST_ASSERT_EQUAL_INT(GS_MSG_CONNECT, gs_at_classify_line("CONNECT 1"));
}

void test_line_parser_ok(void)
{
	const char *s = "OK\r\n";
	gs_msg_id_t id = gs_at_process_chunk((const uint8_t *)s, strlen(s));
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, id);
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, s_last_line_id);
	TEST_ASSERT_TRUE(strstr(s_last_async, "OK") != NULL);
}

void test_bulk_esc_z_frame(void)
{
	/* ESC Z 1 0005 hello */
	uint8_t frame[] = {
		0x1B, 'Z', '1', '0', '0', '0', '5',
		'h', 'e', 'l', 'l', 'o'
	};
	gs_msg_id_t id = gs_at_process_chunk(frame, sizeof(frame));
	TEST_ASSERT_EQUAL_INT(GS_MSG_BULK_DATA, id);
	TEST_ASSERT_EQUAL_UINT8(1, s_rx_cid);
	TEST_ASSERT_EQUAL_INT(GS_ESC_KIND_BULK, s_rx_kind);
	TEST_ASSERT_EQUAL_UINT(5, s_rx_len);
	TEST_ASSERT_EQUAL_MEMORY("hello", s_rx_buf, 5);
}

void test_http_esc_h_frame(void)
{
	uint8_t frame[] = {
		0x1B, 'H', '0', '0', '0', '0', '3',
		'A', 'B', 'C'
	};
	gs_msg_id_t id = gs_at_process_chunk(frame, sizeof(frame));
	TEST_ASSERT_EQUAL_INT(GS_MSG_HTTP_DATA, id);
	TEST_ASSERT_EQUAL_UINT8(0, s_rx_cid);
	TEST_ASSERT_EQUAL_INT(GS_ESC_KIND_HTTP, s_rx_kind);
	TEST_ASSERT_EQUAL_UINT(3, s_rx_len);
	TEST_ASSERT_EQUAL_MEMORY("ABC", s_rx_buf, 3);
}

void test_stream_esc_s_e(void)
{
	uint8_t frame[] = {
		0x1B, 'S', '2', 'x', 'y', 0x1B, 'E'
	};
	gs_msg_id_t id = gs_at_process_chunk(frame, sizeof(frame));
	TEST_ASSERT_EQUAL_INT(GS_MSG_STREAM_DATA, id);
	TEST_ASSERT_EQUAL_UINT8(2, s_rx_cid);
	TEST_ASSERT_EQUAL_INT(GS_ESC_KIND_STREAM, s_rx_kind);
	TEST_ASSERT_EQUAL_UINT(2, s_rx_len);
	TEST_ASSERT_EQUAL_MEMORY("xy", s_rx_buf, 2);
}

void test_esc_ok_fail(void)
{
	uint8_t okf[] = { 0x1B, 'O' };
	uint8_t fail[] = { 0x1B, 'F' };
	TEST_ASSERT_EQUAL_INT(GS_MSG_ESC_OK, gs_at_process_chunk(okf, 2));
	TEST_ASSERT_EQUAL_INT(GS_MSG_ESC_FAIL, gs_at_process_chunk(fail, 2));
}

void test_cid_and_4digit(void)
{
	char d[5];
	TEST_ASSERT_EQUAL_CHAR('A', gs_at_cid_to_ascii(10));
	TEST_ASSERT_EQUAL_UINT8(10, gs_at_ascii_to_cid('A'));
	gs_at_u32_to_4digit(42, d);
	TEST_ASSERT_EQUAL_STRING("0042", d);
	gs_at_u32_to_4digit(10005, d); /* clamped */
	TEST_ASSERT_EQUAL_STRING("9999", d);
}

void test_join_cmd_builder_format(void)
{
	/* Host-side check of command string shape used by wifi join. */
	char buf[64];
	snprintf(buf, sizeof(buf), "AT+WA=%s,,%s\r\n", "TestSSID", "6");
	TEST_ASSERT_EQUAL_STRING("AT+WA=TestSSID,,6\r\n", buf);
	snprintf(buf, sizeof(buf), "AT+WWPA=%s\r\n", "secret12");
	TEST_ASSERT_EQUAL_STRING("AT+WWPA=secret12\r\n", buf);
	snprintf(buf, sizeof(buf), "AT+BDATA=%u\r\n", 1U);
	TEST_ASSERT_EQUAL_STRING("AT+BDATA=1\r\n", buf);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_classify_ok_error);
	RUN_TEST(test_line_parser_ok);
	RUN_TEST(test_bulk_esc_z_frame);
	RUN_TEST(test_http_esc_h_frame);
	RUN_TEST(test_stream_esc_s_e);
	RUN_TEST(test_esc_ok_fail);
	RUN_TEST(test_cid_and_4digit);
	RUN_TEST(test_join_cmd_builder_format);
	return UNITY_END();
}
