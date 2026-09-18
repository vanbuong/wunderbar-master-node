/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_ble_central.h"

#include <string.h>

#include "nordic_common.h"
#include "sdk_common.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_db_discovery.h"
#include "softdevice_handler.h"
#include "wb_sensor_c.h"
#include "wb_nrf51_board.h"
#include "wb_rtt.h"

#define CENTRAL_LINK_COUNT    1
#define PERIPHERAL_LINK_COUNT 0

#define SCAN_INTERVAL 0x00A0
#define SCAN_WINDOW   0x0050

#define MIN_CONN_INTERVAL MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define SLAVE_LATENCY     0
#define SUPERVISION_TO    MSEC_TO_UNITS(4000, UNIT_10_MS)

#define UUID16_SIZE  2
#define UUID32_SIZE  4
#define UUID128_SIZE 16

static ble_db_discovery_t m_db_disc;
static wb_sensor_c_t m_sensor_c;
static wb_ble_evt_handler_t m_handler;
static bool m_scanning;
static bool m_auto_connect;
static bool m_connected;

static const ble_gap_conn_params_t m_conn_params = {
	.min_conn_interval = (uint16_t)MIN_CONN_INTERVAL,
	.max_conn_interval = (uint16_t)MAX_CONN_INTERVAL,
	.slave_latency = (uint16_t)SLAVE_LATENCY,
	.conn_sup_timeout = (uint16_t)SUPERVISION_TO,
};

static const ble_gap_scan_params_t m_scan_params = {
	.active = 1,
	.interval = SCAN_INTERVAL,
	.window = SCAN_WINDOW,
	.timeout = 0,
#if (NRF_SD_BLE_API_VERSION == 2)
	.selective = 0,
	.p_whitelist = NULL,
#endif
};

static void emit(wb_ble_evt_type_t type, const ble_gap_addr_t *peer, int8_t rssi,
		 uint8_t reason, const uint8_t *data, uint16_t len)
{
	wb_ble_evt_t evt;

	if (!m_handler) {
		return;
	}
	memset(&evt, 0, sizeof(evt));
	evt.type = type;
	if (peer) {
		evt.peer = *peer;
	}
	evt.rssi = rssi;
	evt.reason = reason;
	evt.data = data;
	evt.data_len = len;
	m_handler(&evt);
}

static bool is_uuid_present(const ble_uuid_t *target,
			    const ble_gap_evt_adv_report_t *report)
{
	uint32_t index = 0;
	uint8_t *p_data = (uint8_t *)report->data;

	while (index < report->dlen) {
		uint8_t field_length = p_data[index];
		uint8_t field_type;
		ble_uuid_t extracted;
		uint32_t err;

		if (field_length == 0) {
			break;
		}
		field_type = p_data[index + 1];

		if (field_type == BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_MORE_AVAILABLE ||
		    field_type == BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE) {
			uint32_t u;

			for (u = 0; u < (field_length / UUID16_SIZE); u++) {
				err = sd_ble_uuid_decode(
					UUID16_SIZE,
					&p_data[u * UUID16_SIZE + index + 2],
					&extracted);
				if (err == NRF_SUCCESS &&
				    extracted.uuid == target->uuid &&
				    extracted.type == target->type) {
					return true;
				}
			}
		} else if (field_type ==
				   BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE ||
			   field_type ==
				   BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_COMPLETE) {
			err = sd_ble_uuid_decode(UUID128_SIZE,
						 &p_data[index + 2], &extracted);
			if (err == NRF_SUCCESS && extracted.uuid == target->uuid &&
			    extracted.type == target->type) {
				return true;
			}
		}

		index += field_length + 1;
	}
	return false;
}

static void sensor_c_evt_handler(wb_sensor_c_t *p_c,
				 const wb_sensor_c_evt_t *p_evt)
{
	uint32_t err;

	switch (p_evt->evt_type) {
	case WB_SENSOR_C_EVT_DISCOVERY_COMPLETE:
		err = wb_sensor_c_handles_assign(p_c, p_evt->conn_handle,
						 &p_evt->handles);
		APP_ERROR_CHECK(err);
		err = wb_sensor_c_notif_enable(p_c);
		APP_ERROR_CHECK(err);
		WB_RTT_PRINTF("BLE: GATT ready, notify on\r\n");
		emit(WB_BLE_EVT_READY, NULL, 0, 0, NULL, 0);
		break;
	case WB_SENSOR_C_EVT_DATA:
		emit(WB_BLE_EVT_DATA, NULL, 0, 0, p_evt->p_data, p_evt->data_len);
		break;
	case WB_SENSOR_C_EVT_DISCONNECTED:
		m_connected = false;
		WB_RTT_PRINTF("BLE: sensor client disconnected\r\n");
		break;
	default:
		break;
	}
}

static void db_disc_handler(ble_db_discovery_evt_t *p_evt)
{
	wb_sensor_c_on_db_disc_evt(&m_sensor_c, p_evt);
}

static void on_ble_evt(ble_evt_t *p_ble_evt)
{
	uint32_t err;
	const ble_gap_evt_t *gap = &p_ble_evt->evt.gap_evt;
	ble_uuid_t svc_uuid;

	svc_uuid.type = wb_sensor_c_uuid_type(&m_sensor_c);
	svc_uuid.uuid = WB_BT_UUID_SERVICE;

	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT: {
		const ble_gap_evt_adv_report_t *adv = &gap->params.adv_report;

		if (!is_uuid_present(&svc_uuid, adv)) {
			break;
		}

		emit(WB_BLE_EVT_SCAN_REPORT, &adv->peer_addr, adv->rssi, 0, NULL,
		     0);
		WB_RTT_PRINTF("BLE: scan hit rssi=%d\r\n", (int)adv->rssi);

		if (m_auto_connect && !m_connected) {
			err = sd_ble_gap_connect(&adv->peer_addr, &m_scan_params,
						 &m_conn_params);
			if (err == NRF_SUCCESS) {
				m_scanning = false;
				WB_RTT_PRINTF("BLE: auto-connecting\r\n");
			}
		}
		break;
	}
	case BLE_GAP_EVT_CONNECTED:
		m_connected = true;
		m_scanning = false;
		WB_RTT_PRINTF("BLE: connected\r\n");
		emit(WB_BLE_EVT_CONNECTED, &gap->params.connected.peer_addr, 0,
		     0, NULL, 0);
		err = ble_db_discovery_start(&m_db_disc, gap->conn_handle);
		APP_ERROR_CHECK(err);
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		m_connected = false;
		WB_RTT_PRINTF("BLE: disconnected reason=0x%02X\r\n",
			      gap->params.disconnected.reason);
		emit(WB_BLE_EVT_DISCONNECTED, NULL, 0,
		     gap->params.disconnected.reason, NULL, 0);
		break;
	case BLE_GAP_EVT_TIMEOUT:
		if (gap->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) {
			m_scanning = false;
		} else if (gap->params.timeout.src ==
			   BLE_GAP_TIMEOUT_SRC_CONN) {
			WB_RTT_PRINTF("BLE: connect timeout\r\n");
		}
		break;
	default:
		break;
	}
}

static void ble_evt_dispatch(ble_evt_t *p_ble_evt)
{
	on_ble_evt(p_ble_evt);
	ble_db_discovery_on_ble_evt(&m_db_disc, p_ble_evt);
	wb_sensor_c_on_ble_evt(&m_sensor_c, p_ble_evt);
}

static uint32_t ble_stack_init(void)
{
	uint32_t err;
	nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
	ble_enable_params_t ble_enable_params;

	SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

	err = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
						   PERIPHERAL_LINK_COUNT,
						   &ble_enable_params);
	VERIFY_SUCCESS(err);

	CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);

	/* One vendor UUID base for WBS. */
	ble_enable_params.common_enable_params.vs_uuid_count = 1;

	err = softdevice_enable(&ble_enable_params);
	VERIFY_SUCCESS(err);

	return softdevice_ble_evt_handler_set(ble_evt_dispatch);
}

uint32_t wb_ble_central_init(wb_ble_evt_handler_t handler)
{
	uint32_t err;
	wb_sensor_c_init_t init;

	m_handler = handler;
	m_scanning = false;
	m_auto_connect = false;
	m_connected = false;

	err = ble_stack_init();
	VERIFY_SUCCESS(err);

	err = ble_db_discovery_init(db_disc_handler);
	VERIFY_SUCCESS(err);

	init.evt_handler = sensor_c_evt_handler;
	err = wb_sensor_c_init(&m_sensor_c, &init);
	VERIFY_SUCCESS(err);

	WB_RTT_PRINTF("BLE: S130 Central ready\r\n");
	return NRF_SUCCESS;
}

uint32_t wb_ble_scan_start(bool auto_connect)
{
	uint32_t err;

	m_auto_connect = auto_connect;
	err = sd_ble_gap_scan_start(&m_scan_params);
	if (err == NRF_SUCCESS) {
		m_scanning = true;
		WB_RTT_PRINTF("BLE: scan start auto=%u\r\n",
			      (unsigned)auto_connect);
	}
	return err;
}

uint32_t wb_ble_scan_stop(void)
{
	uint32_t err = sd_ble_gap_scan_stop();

	m_scanning = false;
	m_auto_connect = false;
	if (err == NRF_ERROR_INVALID_STATE) {
		return NRF_SUCCESS;
	}
	return err;
}

uint32_t wb_ble_connect(const ble_gap_addr_t *addr)
{
	if (!addr) {
		return NRF_ERROR_NULL;
	}
	(void)wb_ble_scan_stop();
	return sd_ble_gap_connect(addr, &m_scan_params, &m_conn_params);
}

uint32_t wb_ble_disconnect(void)
{
	if (m_sensor_c.conn_handle == BLE_CONN_HANDLE_INVALID) {
		return NRF_ERROR_INVALID_STATE;
	}
	return sd_ble_gap_disconnect(m_sensor_c.conn_handle,
				     BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
}

bool wb_ble_is_connected(void)
{
	return m_connected;
}

bool wb_ble_is_scanning(void)
{
	return m_scanning;
}
