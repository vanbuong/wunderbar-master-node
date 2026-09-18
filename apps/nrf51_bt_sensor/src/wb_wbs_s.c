/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_wbs_s.h"

#include <string.h>

#include "ble_srv_common.h"
#include "sdk_common.h"
#include "wb_bt_gatt.h"

uint32_t wb_wbs_s_init(wb_wbs_s_t *p_wbs)
{
	uint32_t err;
	ble_uuid_t ble_uuid;
	ble_uuid128_t base = WB_BT_UUID_BASE;
	ble_add_char_params_t add_char;

	VERIFY_PARAM_NOT_NULL(p_wbs);
	memset(p_wbs, 0, sizeof(*p_wbs));
	p_wbs->conn_handle = BLE_CONN_HANDLE_INVALID;

	err = sd_ble_uuid_vs_add(&base, &p_wbs->uuid_type);
	VERIFY_SUCCESS(err);

	ble_uuid.type = p_wbs->uuid_type;
	ble_uuid.uuid = WB_BT_UUID_SERVICE;
	err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid,
				       &p_wbs->service_handle);
	VERIFY_SUCCESS(err);

	memset(&add_char, 0, sizeof(add_char));
	add_char.uuid = WB_BT_UUID_DATA;
	add_char.uuid_type = p_wbs->uuid_type;
	add_char.max_len = sizeof(wb_bt_imu_sample_t);
	add_char.init_len = 0;
	add_char.p_init_value = NULL;
	add_char.is_var_len = true;
	add_char.char_props.notify = 1;
	add_char.char_props.read = 1;
	add_char.cccd_write_access = SEC_OPEN;
	add_char.read_access = SEC_OPEN;
	err = characteristic_add(p_wbs->service_handle, &add_char,
				 &p_wbs->data_handles);
	VERIFY_SUCCESS(err);

	memset(&add_char, 0, sizeof(add_char));
	add_char.uuid = WB_BT_UUID_CONFIG;
	add_char.uuid_type = p_wbs->uuid_type;
	add_char.max_len = 8;
	add_char.init_len = 1;
	add_char.char_props.write = 1;
	add_char.char_props.write_wo_resp = 1;
	add_char.write_access = SEC_OPEN;
	err = characteristic_add(p_wbs->service_handle, &add_char,
				 &p_wbs->config_handles);
	VERIFY_SUCCESS(err);

	return NRF_SUCCESS;
}

void wb_wbs_s_on_ble_evt(wb_wbs_s_t *p_wbs, ble_evt_t *p_ble_evt)
{
	if (!p_wbs || !p_ble_evt) {
		return;
	}

	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		p_wbs->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		p_wbs->conn_handle = BLE_CONN_HANDLE_INVALID;
		p_wbs->notify_enabled = false;
		break;
	case BLE_GATTS_EVT_WRITE: {
		ble_gatts_evt_write_t *wr = &p_ble_evt->evt.gatts_evt.params.write;

		if (wr->handle == p_wbs->data_handles.cccd_handle &&
		    wr->len == 2) {
			p_wbs->notify_enabled = ble_srv_is_notification_enabled(wr->data);
		}
		break;
	}
	default:
		break;
	}
}

uint32_t wb_wbs_s_notify(wb_wbs_s_t *p_wbs, const wb_bt_imu_sample_t *sample)
{
	ble_gatts_hvx_params_t hvx;
	uint16_t len;

	VERIFY_PARAM_NOT_NULL(p_wbs);
	VERIFY_PARAM_NOT_NULL(sample);

	if (p_wbs->conn_handle == BLE_CONN_HANDLE_INVALID ||
	    !p_wbs->notify_enabled) {
		return NRF_ERROR_INVALID_STATE;
	}

	len = sizeof(*sample);
	memset(&hvx, 0, sizeof(hvx));
	hvx.handle = p_wbs->data_handles.value_handle;
	hvx.type = BLE_GATT_HVX_NOTIFICATION;
	hvx.p_len = &len;
	hvx.p_data = (uint8_t *)sample;
	return sd_ble_gatts_hvx(p_wbs->conn_handle, &hvx);
}

uint8_t wb_wbs_s_uuid_type(const wb_wbs_s_t *p_wbs)
{
	return p_wbs ? p_wbs->uuid_type : 0;
}

bool wb_wbs_s_notify_enabled(const wb_wbs_s_t *p_wbs)
{
	return p_wbs && p_wbs->notify_enabled;
}
