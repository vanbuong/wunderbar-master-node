/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_sensor_c.h"

#include <string.h>

#include "ble_gattc.h"
#include "ble_srv_common.h"
#include "sdk_common.h"
#include "app_error.h"

void wb_sensor_c_on_db_disc_evt(wb_sensor_c_t *p_c, ble_db_discovery_evt_t *p_evt)
{
	wb_sensor_c_evt_t evt;
	ble_gatt_db_char_t *chars;
	uint32_t i;

	if (!p_c || !p_evt) {
		return;
	}

	memset(&evt, 0, sizeof(evt));
	chars = p_evt->params.discovered_db.charateristics;

	if (p_evt->evt_type != BLE_DB_DISCOVERY_COMPLETE ||
	    p_evt->params.discovered_db.srv_uuid.uuid != WB_BT_UUID_SERVICE ||
	    p_evt->params.discovered_db.srv_uuid.type != p_c->uuid_type) {
		return;
	}

	for (i = 0; i < p_evt->params.discovered_db.char_count; i++) {
		switch (chars[i].characteristic.uuid.uuid) {
		case WB_BT_UUID_DATA:
			evt.handles.data_handle =
				chars[i].characteristic.handle_value;
			evt.handles.data_cccd_handle = chars[i].cccd_handle;
			break;
		case WB_BT_UUID_CONFIG:
			evt.handles.config_handle =
				chars[i].characteristic.handle_value;
			break;
		default:
			break;
		}
	}

	if (p_c->evt_handler) {
		evt.conn_handle = p_evt->conn_handle;
		evt.evt_type = WB_SENSOR_C_EVT_DISCOVERY_COMPLETE;
		p_c->evt_handler(p_c, &evt);
	}
}

static void on_hvx(wb_sensor_c_t *p_c, const ble_evt_t *p_ble_evt)
{
	wb_sensor_c_evt_t evt;

	if (p_c->handles.data_handle == BLE_GATT_HANDLE_INVALID ||
	    p_ble_evt->evt.gattc_evt.params.hvx.handle !=
		    p_c->handles.data_handle ||
	    !p_c->evt_handler) {
		return;
	}

	evt.evt_type = WB_SENSOR_C_EVT_DATA;
	evt.conn_handle = p_c->conn_handle;
	evt.p_data = (uint8_t *)p_ble_evt->evt.gattc_evt.params.hvx.data;
	evt.data_len = p_ble_evt->evt.gattc_evt.params.hvx.len;
	evt.handles = p_c->handles;
	p_c->evt_handler(p_c, &evt);
}

uint32_t wb_sensor_c_init(wb_sensor_c_t *p_c, const wb_sensor_c_init_t *p_init)
{
	uint32_t err;
	ble_uuid_t svc;
	ble_uuid128_t base = WB_BT_UUID_BASE;

	VERIFY_PARAM_NOT_NULL(p_c);
	VERIFY_PARAM_NOT_NULL(p_init);

	err = sd_ble_uuid_vs_add(&base, &p_c->uuid_type);
	VERIFY_SUCCESS(err);

	svc.type = p_c->uuid_type;
	svc.uuid = WB_BT_UUID_SERVICE;

	p_c->conn_handle = BLE_CONN_HANDLE_INVALID;
	p_c->evt_handler = p_init->evt_handler;
	p_c->handles.data_handle = BLE_GATT_HANDLE_INVALID;
	p_c->handles.data_cccd_handle = BLE_GATT_HANDLE_INVALID;
	p_c->handles.config_handle = BLE_GATT_HANDLE_INVALID;

	return ble_db_discovery_evt_register(&svc);
}

void wb_sensor_c_on_ble_evt(wb_sensor_c_t *p_c, const ble_evt_t *p_ble_evt)
{
	if (!p_c || !p_ble_evt) {
		return;
	}

	if (p_c->conn_handle != BLE_CONN_HANDLE_INVALID &&
	    p_c->conn_handle != p_ble_evt->evt.gap_evt.conn_handle) {
		return;
	}

	switch (p_ble_evt->header.evt_id) {
	case BLE_GATTC_EVT_HVX:
		on_hvx(p_c, p_ble_evt);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		if (p_ble_evt->evt.gap_evt.conn_handle == p_c->conn_handle &&
		    p_c->evt_handler) {
			wb_sensor_c_evt_t evt;

			evt.evt_type = WB_SENSOR_C_EVT_DISCONNECTED;
			evt.conn_handle = p_c->conn_handle;
			evt.p_data = NULL;
			evt.data_len = 0;
			p_c->conn_handle = BLE_CONN_HANDLE_INVALID;
			p_c->evt_handler(p_c, &evt);
		}
		break;
	default:
		break;
	}
}

uint32_t wb_sensor_c_handles_assign(wb_sensor_c_t *p_c, uint16_t conn_handle,
				    const wb_sensor_c_handles_t *p_handles)
{
	VERIFY_PARAM_NOT_NULL(p_c);

	p_c->conn_handle = conn_handle;
	if (p_handles) {
		p_c->handles = *p_handles;
	}
	return NRF_SUCCESS;
}

static uint32_t cccd_configure(uint16_t conn_handle, uint16_t cccd_handle,
			       bool enable)
{
	uint8_t buf[BLE_CCCD_VALUE_LEN];

	buf[0] = enable ? BLE_GATT_HVX_NOTIFICATION : 0;
	buf[1] = 0;

	const ble_gattc_write_params_t write_params = {
		.write_op = BLE_GATT_OP_WRITE_REQ,
		.flags = BLE_GATT_EXEC_WRITE_FLAG_PREPARED_WRITE,
		.handle = cccd_handle,
		.offset = 0,
		.len = sizeof(buf),
		.p_value = buf,
	};

	return sd_ble_gattc_write(conn_handle, &write_params);
}

uint32_t wb_sensor_c_notif_enable(wb_sensor_c_t *p_c)
{
	VERIFY_PARAM_NOT_NULL(p_c);

	if (p_c->conn_handle == BLE_CONN_HANDLE_INVALID ||
	    p_c->handles.data_cccd_handle == BLE_GATT_HANDLE_INVALID) {
		return NRF_ERROR_INVALID_STATE;
	}
	return cccd_configure(p_c->conn_handle, p_c->handles.data_cccd_handle,
			      true);
}

uint8_t wb_sensor_c_uuid_type(const wb_sensor_c_t *p_c)
{
	return p_c ? p_c->uuid_type : 0;
}
