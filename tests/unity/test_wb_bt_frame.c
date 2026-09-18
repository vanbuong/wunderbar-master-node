/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "wb_bt_frame.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_frame_size_is_64(void)
{
	TEST_ASSERT_EQUAL_UINT(64u, sizeof(wb_bt_frame_t));
	TEST_ASSERT_EQUAL_UINT(WB_BT_FRAME_SIZE, sizeof(wb_bt_frame_t));
}

void test_frame_roundtrip_crc(void)
{
	wb_bt_frame_t f;
	uint8_t msg[] = { 'h', 'i' };

	wb_bt_frame_init(&f);
	f.type = WB_BT_TYPE_DATA;
	f.sensor_id = 1;
	f.field_id = 2;
	f.seq = 7;
	f.payload_len = sizeof(msg);
	memcpy(f.payload, msg, sizeof(msg));
	wb_bt_frame_finalize(&f);

	TEST_ASSERT_TRUE(wb_bt_frame_valid(&f));
	f.payload[0] ^= 0x01;
	TEST_ASSERT_FALSE(wb_bt_frame_valid(&f));
}

void test_idle_ping_types(void)
{
	wb_bt_frame_t f;

	wb_bt_frame_init(&f);
	f.type = WB_BT_TYPE_PING;
	wb_bt_frame_finalize(&f);
	TEST_ASSERT_TRUE(wb_bt_frame_valid(&f));
	TEST_ASSERT_EQUAL_UINT8(WB_BT_FRAME_VERSION, f.version);
	TEST_ASSERT_EQUAL_UINT8('W', f.magic[0]);
	TEST_ASSERT_EQUAL_UINT8('T', f.magic[3]);
}

void test_frame_make_helpers(void)
{
	wb_bt_frame_t f;
	uint8_t pl[] = { WB_BT_CAP_SPI, 'o', 'k', '\0' };

	wb_bt_frame_make(&f, WB_BT_TYPE_RSP, 3, WB_BT_CMD_GET_INFO, pl,
			 sizeof(pl));
	TEST_ASSERT_TRUE(wb_bt_frame_valid(&f));
	TEST_ASSERT_EQUAL_UINT8(WB_BT_TYPE_RSP, f.type);
	TEST_ASSERT_EQUAL_UINT8(3, f.seq);
	TEST_ASSERT_EQUAL_UINT8(WB_BT_CMD_GET_INFO, f.field_id);
	TEST_ASSERT_EQUAL_UINT16(sizeof(pl), f.payload_len);
	TEST_ASSERT_EQUAL_UINT8(WB_BT_CAP_SPI, f.payload[0]);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_frame_size_is_64);
	RUN_TEST(test_frame_roundtrip_crc);
	RUN_TEST(test_idle_ping_types);
	RUN_TEST(test_frame_make_helpers);
	return UNITY_END();
}
