/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * SoftDevice S130 Central orchestration (scan / connect / notify path).
 */

#ifndef WB_BLE_CENTRAL_H
#define WB_BLE_CENTRAL_H

#include <stdint.h>
#include <stdbool.h>

#include "ble_gap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	WB_BLE_EVT_SCAN_REPORT = 1,
	WB_BLE_EVT_CONNECTED,
	WB_BLE_EVT_DISCONNECTED,
	WB_BLE_EVT_READY,
	WB_BLE_EVT_DATA,
} wb_ble_evt_type_t;

typedef struct {
	wb_ble_evt_type_t type;
	ble_gap_addr_t peer;
	int8_t rssi;
	uint8_t reason;
	const uint8_t *data;
	uint16_t data_len;
} wb_ble_evt_t;

typedef void (*wb_ble_evt_handler_t)(const wb_ble_evt_t *evt);

/** Init SoftDevice + DB discovery + sensor GATT client. */
uint32_t wb_ble_central_init(wb_ble_evt_handler_t handler);

uint32_t wb_ble_scan_start(bool auto_connect);
uint32_t wb_ble_scan_stop(void);
uint32_t wb_ble_connect(const ble_gap_addr_t *addr);
uint32_t wb_ble_disconnect(void);

bool wb_ble_is_connected(void);
bool wb_ble_is_scanning(void);

#ifdef __cplusplus
}
#endif

#endif /* WB_BLE_CENTRAL_H */
