/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * WunderBar Sensor (WBS) GATT server — matches master's wb_bt_gatt.h.
 */

#ifndef WB_WBS_S_H
#define WB_WBS_S_H

#include <stdint.h>
#include <stdbool.h>

#include "ble.h"
#include "wb_bt_imu.h"

typedef struct {
	uint16_t service_handle;
	ble_gatts_char_handles_t data_handles;
	ble_gatts_char_handles_t config_handles;
	uint16_t conn_handle;
	uint8_t uuid_type;
	bool notify_enabled;
} wb_wbs_s_t;

uint32_t wb_wbs_s_init(wb_wbs_s_t *p_wbs);
void wb_wbs_s_on_ble_evt(wb_wbs_s_t *p_wbs, ble_evt_t *p_ble_evt);
uint32_t wb_wbs_s_notify(wb_wbs_s_t *p_wbs, const wb_bt_imu_sample_t *sample);
uint8_t wb_wbs_s_uuid_type(const wb_wbs_s_t *p_wbs);
bool wb_wbs_s_notify_enabled(const wb_wbs_s_t *p_wbs);

#endif /* WB_WBS_S_H */
