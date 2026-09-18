/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * WunderBar BT host ↔ nRF51822 SPI frame (ABI v1).
 * Shared by MK24 host and nRF slave firmware.
 */

#ifndef WB_BT_FRAME_H
#define WB_BT_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WB_BT_FRAME_MAGIC0   'W'
#define WB_BT_FRAME_MAGIC1   'B'
#define WB_BT_FRAME_MAGIC2   'B'
#define WB_BT_FRAME_MAGIC3   'T'
#define WB_BT_FRAME_VERSION  1u
#define WB_BT_PAYLOAD_MAX    48u
#define WB_BT_FRAME_SIZE     64u

#define WB_BT_SENSOR_NONE    0xFFu

typedef enum {
	WB_BT_TYPE_IDLE = 0x00,
	WB_BT_TYPE_PING = 0x01,
	WB_BT_TYPE_PONG = 0x02,
	WB_BT_TYPE_CMD  = 0x10,
	WB_BT_TYPE_RSP  = 0x11,
	WB_BT_TYPE_EVT  = 0x20,
	WB_BT_TYPE_DATA = 0x21,
} wb_bt_type_t;

typedef enum {
	WB_BT_CMD_GET_INFO      = 0x01,
	WB_BT_CMD_SCAN_START    = 0x02,
	WB_BT_CMD_SCAN_STOP     = 0x03,
	WB_BT_CMD_CONNECT       = 0x04,
	WB_BT_CMD_DISCONNECT    = 0x05,
	WB_BT_CMD_SET_SENSOR_CFG = 0x06,
} wb_bt_cmd_t;

typedef struct __attribute__((packed)) {
	uint8_t magic[4];
	uint8_t version;
	uint8_t flags;
	uint8_t seq;
	uint8_t type;
	uint8_t sensor_id;
	uint8_t field_id;
	uint16_t payload_len;
	uint8_t payload[WB_BT_PAYLOAD_MAX];
	uint8_t reserved[2]; /* pad to WB_BT_FRAME_SIZE */
	uint16_t crc16;
} wb_bt_frame_t;

_Static_assert(sizeof(wb_bt_frame_t) == WB_BT_FRAME_SIZE,
	       "wb_bt_frame_t must be 64 bytes");

/** CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). */
static inline uint16_t wb_bt_crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = 0xFFFFu;
	size_t i;
	int b;

	for (i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i] << 8;
		for (b = 0; b < 8; b++) {
			if (crc & 0x8000u) {
				crc = (uint16_t)((crc << 1) ^ 0x1021u);
			} else {
				crc <<= 1;
			}
		}
	}
	return crc;
}

static inline void wb_bt_frame_init(wb_bt_frame_t *f)
{
	size_t i;

	if (!f) {
		return;
	}
	for (i = 0; i < sizeof(*f); i++) {
		((uint8_t *)f)[i] = 0;
	}
	f->magic[0] = WB_BT_FRAME_MAGIC0;
	f->magic[1] = WB_BT_FRAME_MAGIC1;
	f->magic[2] = WB_BT_FRAME_MAGIC2;
	f->magic[3] = WB_BT_FRAME_MAGIC3;
	f->version = WB_BT_FRAME_VERSION;
	f->sensor_id = WB_BT_SENSOR_NONE;
}

static inline void wb_bt_frame_finalize(wb_bt_frame_t *f)
{
	uint16_t crc;

	if (!f) {
		return;
	}
	if (f->payload_len > WB_BT_PAYLOAD_MAX) {
		f->payload_len = WB_BT_PAYLOAD_MAX;
	}
	crc = wb_bt_crc16((const uint8_t *)f,
			  offsetof(wb_bt_frame_t, crc16));
	f->crc16 = crc;
}

static inline bool wb_bt_frame_valid(const wb_bt_frame_t *f)
{
	uint16_t crc;

	if (!f) {
		return false;
	}
	if (f->magic[0] != WB_BT_FRAME_MAGIC0 ||
	    f->magic[1] != WB_BT_FRAME_MAGIC1 ||
	    f->magic[2] != WB_BT_FRAME_MAGIC2 ||
	    f->magic[3] != WB_BT_FRAME_MAGIC3) {
		return false;
	}
	if (f->version != WB_BT_FRAME_VERSION) {
		return false;
	}
	if (f->payload_len > WB_BT_PAYLOAD_MAX) {
		return false;
	}
	crc = wb_bt_crc16((const uint8_t *)f,
			  offsetof(wb_bt_frame_t, crc16));
	return crc == f->crc16;
}

#ifdef __cplusplus
}
#endif

#endif /* WB_BT_FRAME_H */
