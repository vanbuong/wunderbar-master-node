/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * nRF51822 IMU sensor peripheral: MPU-6xxx over I2C, complementary AHRS,
 * SoftDevice S130 advertises WBS GATT and notifies roll/pitch/yaw + motion.
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "app_timer.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_gap.h"
#include "softdevice_handler.h"
#include "nordic_common.h"

#include "wb_sensor_board.h"
#include "wb_rtt.h"
#include "wb_twi.h"
#include "wb_mpu.h"
#include "wb_ahrs.h"
#include "wb_wbs_s.h"
#include "wb_bt_gatt.h"
#include "wb_bt_imu.h"

#define DEVICE_NAME                "WB-IMU"
#define WB_FW_ID                   "wb-nrf51-sensor"

#define CENTRAL_LINK_COUNT         0
#define PERIPHERAL_LINK_COUNT      1

#define APP_ADV_INTERVAL           160 /* 100 ms (units of 0.625 ms) */
#define APP_ADV_TIMEOUT_SECONDS    0   /* never */

#define MIN_CONN_INTERVAL          MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL          MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define SLAVE_LATENCY              0
#define CONN_SUP_TIMEOUT           MSEC_TO_UNITS(4000, UNIT_10_MS)

#define APP_TIMER_PRESCALER        0
#define APP_TIMER_OP_QUEUE_SIZE    4
#define IMU_SAMPLE_MS              20 /* 50 Hz */

#define DEAD_BEEF                  0xDEADBEEF

static wb_wbs_s_t m_wbs;
static ble_gap_adv_params_t m_adv_params;
static ble_uuid_t m_adv_uuids[1];
static volatile bool m_connected;
static volatile bool m_imu_tick;
static bool m_imu_ok;
static uint32_t m_notify_ok;
static uint32_t m_notify_fail;

APP_TIMER_DEF(m_imu_timer);

void assert_nrf_callback(uint16_t line_num, const uint8_t *p_file_name)
{
	app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

static void gpio_init(void)
{
	nrf_gpio_cfg_output(WB_PIN_LED);
	nrf_gpio_cfg_input(WB_PIN_BTN2, NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_input(WB_PIN_GYRO_INT, NRF_GPIO_PIN_NOPULL);
	WB_LED_OFF();
}

static void gap_params_init(void)
{
	uint32_t err;
	ble_gap_conn_params_t gap_conn_params;
	ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
	err = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME,
					 strlen(DEVICE_NAME));
	APP_ERROR_CHECK(err);

	memset(&gap_conn_params, 0, sizeof(gap_conn_params));
	gap_conn_params.min_conn_interval = (uint16_t)MIN_CONN_INTERVAL;
	gap_conn_params.max_conn_interval = (uint16_t)MAX_CONN_INTERVAL;
	gap_conn_params.slave_latency = (uint16_t)SLAVE_LATENCY;
	gap_conn_params.conn_sup_timeout = (uint16_t)CONN_SUP_TIMEOUT;
	err = sd_ble_gap_ppcp_set(&gap_conn_params);
	APP_ERROR_CHECK(err);
}

static void advertising_init(void)
{
	uint32_t err;
	ble_advdata_t advdata;
	ble_advdata_t scanrsp;
	uint8_t flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

	m_adv_uuids[0].uuid = WB_BT_UUID_SERVICE;
	m_adv_uuids[0].type = wb_wbs_s_uuid_type(&m_wbs);

	memset(&advdata, 0, sizeof(advdata));
	advdata.name_type = BLE_ADVDATA_NO_NAME;
	advdata.flags = flags;
	advdata.uuids_complete.uuid_cnt = 1;
	advdata.uuids_complete.p_uuids = m_adv_uuids;

	memset(&scanrsp, 0, sizeof(scanrsp));
	scanrsp.name_type = BLE_ADVDATA_FULL_NAME;

	err = ble_advdata_set(&advdata, &scanrsp);
	APP_ERROR_CHECK(err);

	memset(&m_adv_params, 0, sizeof(m_adv_params));
	m_adv_params.type = BLE_GAP_ADV_TYPE_ADV_IND;
	m_adv_params.p_peer_addr = NULL;
	m_adv_params.fp = BLE_GAP_ADV_FP_ANY;
	m_adv_params.interval = APP_ADV_INTERVAL;
	m_adv_params.timeout = APP_ADV_TIMEOUT_SECONDS;
}

static void advertising_start(void)
{
	uint32_t err = sd_ble_gap_adv_start(&m_adv_params);

	if (err == NRF_SUCCESS) {
		WB_RTT_PRINTF("ADV: started as %s\r\n", DEVICE_NAME);
	} else {
		WB_RTT_PRINTF("ADV: start fail %lu\r\n", (unsigned long)err);
	}
}

static void on_ble_evt(ble_evt_t *p_ble_evt)
{
	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		m_connected = true;
		WB_LED_ON();
		WB_RTT_PRINTF("BLE: connected\r\n");
		break;
	case BLE_GAP_EVT_DISCONNECTED:
		m_connected = false;
		WB_LED_OFF();
		WB_RTT_PRINTF("BLE: disconnected reason=0x%02X\r\n",
			      p_ble_evt->evt.gap_evt.params.disconnected.reason);
		advertising_start();
		break;
	case BLE_GAP_EVT_TIMEOUT:
		if (p_ble_evt->evt.gap_evt.params.timeout.src ==
		    BLE_GAP_TIMEOUT_SRC_ADVERTISING) {
			advertising_start();
		}
		break;
	case BLE_GATTS_EVT_SYS_ATTR_MISSING:
		(void)sd_ble_gatts_sys_attr_set(
			p_ble_evt->evt.gatts_evt.conn_handle, NULL, 0, 0);
		break;
	default:
		break;
	}
}

static void ble_evt_dispatch(ble_evt_t *p_ble_evt)
{
	on_ble_evt(p_ble_evt);
	wb_wbs_s_on_ble_evt(&m_wbs, p_ble_evt);
}

static void ble_stack_init(void)
{
	uint32_t err;
	nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
	ble_enable_params_t ble_enable_params;

	SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

	err = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT,
						   PERIPHERAL_LINK_COUNT,
						   &ble_enable_params);
	APP_ERROR_CHECK(err);

	CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);

	ble_enable_params.common_enable_params.vs_uuid_count = 1;

	err = softdevice_enable(&ble_enable_params);
	APP_ERROR_CHECK(err);

	err = softdevice_ble_evt_handler_set(ble_evt_dispatch);
	APP_ERROR_CHECK(err);
}

static void imu_timer_handler(void *p_context)
{
	(void)p_context;
	m_imu_tick = true;
}

static void timers_init(void)
{
	uint32_t err;

	APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_OP_QUEUE_SIZE, false);
	err = app_timer_create(&m_imu_timer, APP_TIMER_MODE_REPEATED,
			       imu_timer_handler);
	APP_ERROR_CHECK(err);
}

static void imu_sample_once(void)
{
	wb_mpu_raw_t raw;
	wb_bt_imu_sample_t sample;
	uint32_t err;
	static const char *const motion_name[] = {
		"still", "moving", "tilt", "face_down", "upside_down",
	};
	static uint8_t last_motion = 0xFFu;
	static uint32_t log_div;

	if (!m_imu_ok) {
		return;
	}

	err = wb_mpu_read(&raw);
	if (err != NRF_SUCCESS) {
		return;
	}

	wb_ahrs_update(&raw, (float)IMU_SAMPLE_MS / 1000.0f);
	wb_ahrs_fill_sample(&sample);

	if (wb_wbs_s_notify_enabled(&m_wbs)) {
		err = wb_wbs_s_notify(&m_wbs, &sample);
		if (err == NRF_SUCCESS) {
			m_notify_ok++;
		} else {
			m_notify_fail++;
		}
	}

	if (sample.motion != last_motion) {
		last_motion = sample.motion;
		WB_RTT_PRINTF(
			"motion=%s roll=%d.%02d pitch=%d.%02d yaw=%d.%02d\r\n",
			(sample.motion < 5u) ? motion_name[sample.motion] : "?",
			(int)(sample.roll_cdeg / 100),
			(int)((sample.roll_cdeg < 0 ? -sample.roll_cdeg :
							     sample.roll_cdeg) %
			      100),
			(int)(sample.pitch_cdeg / 100),
			(int)((sample.pitch_cdeg < 0 ? -sample.pitch_cdeg :
							      sample.pitch_cdeg) %
			      100),
			(int)(sample.yaw_cdeg / 100),
			(int)((sample.yaw_cdeg < 0 ? -sample.yaw_cdeg :
							    sample.yaw_cdeg) %
			      100));
	}

	if (++log_div >= 50u) { /* ~1 Hz when sampling */
		log_div = 0;
		WB_RTT_PRINTF(
			"imu r=%d p=%d y=%d ax=%d ay=%d az=%d ntf=%lu/%lu conn=%u\r\n",
			(int)sample.roll_cdeg, (int)sample.pitch_cdeg,
			(int)sample.yaw_cdeg, (int)sample.ax_mg,
			(int)sample.ay_mg, (int)sample.az_mg,
			(unsigned long)m_notify_ok,
			(unsigned long)m_notify_fail,
			(unsigned)m_connected);
	}
}

static void imu_bringup(void)
{
	uint32_t err;

	err = wb_twi_init();
	if (err != NRF_SUCCESS) {
		WB_RTT_PRINTF("TWI init fail %lu\r\n", (unsigned long)err);
		m_imu_ok = false;
		return;
	}

	err = wb_mpu_init();
	if (err != NRF_SUCCESS) {
		WB_RTT_PRINTF("MPU init fail %lu (check I2C wiring)\r\n",
			      (unsigned long)err);
		m_imu_ok = false;
		return;
	}

	wb_ahrs_init();
	m_imu_ok = true;
	WB_RTT_PRINTF("IMU ready addr=0x%02X whoami=0x%02X\r\n", wb_mpu_addr(),
		      wb_mpu_whoami());
}

int main(void)
{
	uint32_t err;

	wb_rtt_init();
	WB_RTT_PRINTF("%s — WBS peripheral + MPU I2C\r\n", WB_FW_ID);

	gpio_init();
	timers_init();

	ble_stack_init();
	gap_params_init();

	err = wb_wbs_s_init(&m_wbs);
	APP_ERROR_CHECK(err);

	advertising_init();
	advertising_start();

	imu_bringup();

	err = app_timer_start(m_imu_timer,
			      APP_TIMER_TICKS(IMU_SAMPLE_MS, APP_TIMER_PRESCALER),
			      NULL);
	APP_ERROR_CHECK(err);

	WB_RTT_PRINTF("running; advertise WBS UUID, sample %u ms\r\n",
		      (unsigned)IMU_SAMPLE_MS);

	for (;;) {
		if (m_imu_tick) {
			m_imu_tick = false;
			imu_sample_once();
		}

		/* Idle blink when not connected; solid when connected. */
		if (!m_connected) {
			static uint32_t blink_div;

			if (++blink_div >= 25u) { /* ~0.5 s at 50 Hz wake */
				blink_div = 0;
				WB_LED_TOGGLE();
			}
		}

		(void)sd_app_evt_wait();
	}
}
