/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Thin GATT client for the greenfield WunderBar sensor service.
 */

#ifndef WB_SENSOR_C_H
#define WB_SENSOR_C_H

#include <stdint.h>
#include <stdbool.h>

#include "ble.h"
#include "ble_db_discovery.h"
#include "wb_bt_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	WB_SENSOR_C_EVT_DISCOVERY_COMPLETE = 1,
	WB_SENSOR_C_EVT_DATA,
	WB_SENSOR_C_EVT_DISCONNECTED,
} wb_sensor_c_evt_type_t;

typedef struct {
	uint16_t data_handle;
	uint16_t data_cccd_handle;
	uint16_t config_handle;
} wb_sensor_c_handles_t;

typedef struct {
	wb_sensor_c_evt_type_t evt_type;
	uint16_t conn_handle;
	uint8_t *p_data;
	uint16_t data_len;
	wb_sensor_c_handles_t handles;
} wb_sensor_c_evt_t;

typedef struct wb_sensor_c_s wb_sensor_c_t;

typedef void (*wb_sensor_c_evt_handler_t)(wb_sensor_c_t *p_c,
					  const wb_sensor_c_evt_t *p_evt);

struct wb_sensor_c_s {
	uint16_t conn_handle;
	uint8_t uuid_type;
	wb_sensor_c_handles_t handles;
	wb_sensor_c_evt_handler_t evt_handler;
};

typedef struct {
	wb_sensor_c_evt_handler_t evt_handler;
} wb_sensor_c_init_t;

uint32_t wb_sensor_c_init(wb_sensor_c_t *p_c, const wb_sensor_c_init_t *p_init);
void wb_sensor_c_on_db_disc_evt(wb_sensor_c_t *p_c, ble_db_discovery_evt_t *p_evt);
void wb_sensor_c_on_ble_evt(wb_sensor_c_t *p_c, const ble_evt_t *p_ble_evt);
uint32_t wb_sensor_c_handles_assign(wb_sensor_c_t *p_c, uint16_t conn_handle,
				    const wb_sensor_c_handles_t *p_handles);
uint32_t wb_sensor_c_notif_enable(wb_sensor_c_t *p_c);
uint8_t wb_sensor_c_uuid_type(const wb_sensor_c_t *p_c);

#ifdef __cplusplus
}
#endif

#endif /* WB_SENSOR_C_H */
