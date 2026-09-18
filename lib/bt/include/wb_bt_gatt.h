/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Greenfield WunderBar sensor GATT UUIDs + SPI EVT field IDs (Phase 2).
 *
 * Service UUID (128-bit): 57420001-4253-1000-8000-00805f9b34fb
 *   short service  = 0x0001
 *   data (notify)  = 0x0002
 *   config (write) = 0x0003
 */

#ifndef WB_BT_GATT_H
#define WB_BT_GATT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Nordic-style base (bytes 12–13 overwritten by 16-bit short UUID). */
#define WB_BT_UUID_BASE                                                            \
	{                                                                          \
		{                                                                  \
			0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, \
			0x53, 0x42, 0x00, 0x00, 0x42, 0x57                          \
		}                                                                  \
	}

#define WB_BT_UUID_SERVICE 0x0001u
#define WB_BT_UUID_DATA    0x0002u /* notify from sensor */
#define WB_BT_UUID_CONFIG  0x0003u /* write to sensor */

/** EVT.field_id values (type = WB_BT_TYPE_EVT). */
typedef enum {
	WB_BT_EVT_SCAN_REPORT  = 0x01,
	WB_BT_EVT_CONNECTED    = 0x02,
	WB_BT_EVT_DISCONNECTED = 0x03,
	WB_BT_EVT_READY        = 0x04, /* GATT discovered + notify enabled */
} wb_bt_evt_id_t;

/** SCAN_START payload[0] flags. */
#define WB_BT_SCAN_FLAG_AUTO_CONNECT 0x01u

/** Single-link Phase 2 sensor id. */
#define WB_BT_SENSOR_ID0 0u

#ifdef __cplusplus
}
#endif

#endif /* WB_BT_GATT_H */
