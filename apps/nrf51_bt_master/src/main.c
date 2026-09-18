/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Phase 2: SPIS host protocol + SoftDevice S130 BLE Central (1 link).
 * Flash SoftDevice first (make flash_sd), then the app (make flash).
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "nrf_drv_spis.h"
#include "ble_gap.h"

#include "wb_nrf51_board.h"
#include "wb_rtt.h"
#include "wb_bt_frame.h"
#include "wb_bt_gatt.h"
#include "wb_txq.h"
#include "wb_ble_central.h"

#define WB_FW_ID "wb-nrf51-p2"

static const nrf_drv_spis_t spis = NRF_DRV_SPIS_INSTANCE(1);

static uint8_t m_tx_buf[WB_BT_FRAME_SIZE];
static uint8_t m_rx_buf[WB_BT_FRAME_SIZE];
static volatile bool m_spis_xfer_done;
static uint8_t m_tx_seq;
static uint32_t m_xfer_count;
static uint32_t m_bad_crc;
static uint32_t m_unknown_cmd;
static bool m_tx_was_queued;

static void ready_from_queue(void)
{
	if (wb_txq_empty()) {
		WB_READY_DEASSERT();
	} else {
		WB_READY_ASSERT();
	}
}

static void load_tx_buffer(void)
{
	wb_bt_frame_t f;

	m_tx_was_queued = wb_txq_peek(&f);
	if (!m_tx_was_queued) {
		wb_bt_frame_make(&f, WB_BT_TYPE_IDLE, m_tx_seq++, 0, NULL, 0);
	}
	memcpy(m_tx_buf, &f, sizeof(f));
}

static bool enqueue_frame(const wb_bt_frame_t *f)
{
	if (!wb_txq_push(f)) {
		WB_RTT_PRINTF("TXQ full, drop type=0x%02X\r\n",
			      (unsigned)f->type);
		return false;
	}
	ready_from_queue();
	return true;
}

static void enqueue_pong(void)
{
	wb_bt_frame_t f;

	wb_bt_frame_make(&f, WB_BT_TYPE_PONG, m_tx_seq++, 0, NULL, 0);
	(void)enqueue_frame(&f);
}

static void enqueue_rsp(uint8_t cmd, const void *payload, uint16_t len)
{
	wb_bt_frame_t f;

	wb_bt_frame_make(&f, WB_BT_TYPE_RSP, m_tx_seq++, cmd, payload, len);
	(void)enqueue_frame(&f);
}

static void enqueue_err_rsp(uint8_t cmd, uint8_t err)
{
	uint8_t pl[1] = { err };

	enqueue_rsp(cmd, pl, sizeof(pl));
}

static void enqueue_evt(uint8_t field_id, uint8_t sensor_id,
			const void *payload, uint16_t len)
{
	wb_bt_frame_t f;

	wb_bt_frame_make(&f, WB_BT_TYPE_EVT, m_tx_seq++, field_id, payload, len);
	f.sensor_id = sensor_id;
	wb_bt_frame_finalize(&f);
	(void)enqueue_frame(&f);
}

static void enqueue_data(const uint8_t *payload, uint16_t len)
{
	wb_bt_frame_t f;

	wb_bt_frame_make(&f, WB_BT_TYPE_DATA, m_tx_seq++, WB_BT_UUID_DATA,
			 payload, len);
	f.sensor_id = WB_BT_SENSOR_ID0;
	wb_bt_frame_finalize(&f);
	(void)enqueue_frame(&f);
}

static void handle_get_info(void)
{
	uint8_t pl[WB_BT_PAYLOAD_MAX];
	size_t id_len = strlen(WB_FW_ID);
	size_t n;

	memset(pl, 0, sizeof(pl));
	pl[0] = (uint8_t)(WB_BT_CAP_SPI | WB_BT_CAP_BLE);
	n = id_len;
	if (n > (sizeof(pl) - 2u)) {
		n = sizeof(pl) - 2u;
	}
	memcpy(&pl[1], WB_FW_ID, n);
	pl[1u + n] = '\0';
	enqueue_rsp(WB_BT_CMD_GET_INFO, pl, (uint16_t)(2u + n));
}

static void handle_scan_start(const wb_bt_frame_t *rx)
{
	bool auto_connect = false;
	uint32_t err;
	uint8_t ok = WB_BT_ERR_OK;

	if (rx->payload_len >= 1u) {
		auto_connect = (rx->payload[0] & WB_BT_SCAN_FLAG_AUTO_CONNECT) != 0;
	}
	err = wb_ble_scan_start(auto_connect);
	if (err != NRF_SUCCESS) {
		ok = WB_BT_ERR_BUSY;
		WB_RTT_PRINTF("SCAN_START fail %lu\r\n", (unsigned long)err);
	}
	enqueue_rsp(WB_BT_CMD_SCAN_START, &ok, 1);
}

static void handle_scan_stop(void)
{
	uint8_t ok = WB_BT_ERR_OK;
	uint32_t err = wb_ble_scan_stop();

	if (err != NRF_SUCCESS) {
		ok = WB_BT_ERR_BUSY;
	}
	enqueue_rsp(WB_BT_CMD_SCAN_STOP, &ok, 1);
}

static void handle_connect(const wb_bt_frame_t *rx)
{
	ble_gap_addr_t addr;
	uint8_t ok = WB_BT_ERR_OK;
	uint32_t err;

	if (rx->payload_len < 7u) {
		enqueue_err_rsp(WB_BT_CMD_CONNECT, WB_BT_ERR_BAD_PARAM);
		return;
	}
	memset(&addr, 0, sizeof(addr));
	addr.addr_type = rx->payload[0];
	memcpy(addr.addr, &rx->payload[1], BLE_GAP_ADDR_LEN);
	err = wb_ble_connect(&addr);
	if (err != NRF_SUCCESS) {
		ok = WB_BT_ERR_BUSY;
		WB_RTT_PRINTF("CONNECT fail %lu\r\n", (unsigned long)err);
	}
	enqueue_rsp(WB_BT_CMD_CONNECT, &ok, 1);
}

static void handle_disconnect(void)
{
	uint8_t ok = WB_BT_ERR_OK;
	uint32_t err = wb_ble_disconnect();

	if (err != NRF_SUCCESS) {
		ok = WB_BT_ERR_BUSY;
	}
	enqueue_rsp(WB_BT_CMD_DISCONNECT, &ok, 1);
}

static void handle_cmd(const wb_bt_frame_t *rx)
{
	switch (rx->field_id) {
	case WB_BT_CMD_GET_INFO:
		handle_get_info();
		break;
	case WB_BT_CMD_SCAN_START:
		handle_scan_start(rx);
		break;
	case WB_BT_CMD_SCAN_STOP:
		handle_scan_stop();
		break;
	case WB_BT_CMD_CONNECT:
		handle_connect(rx);
		break;
	case WB_BT_CMD_DISCONNECT:
		handle_disconnect();
		break;
	case WB_BT_CMD_SET_SENSOR_CFG:
		enqueue_err_rsp(rx->field_id, WB_BT_ERR_BUSY);
		break;
	default:
		m_unknown_cmd++;
		enqueue_err_rsp(rx->field_id, WB_BT_ERR_UNKNOWN_CMD);
		break;
	}
}

static void process_rx_frame(void)
{
	const wb_bt_frame_t *rx = (const wb_bt_frame_t *)m_rx_buf;

	if (!wb_bt_frame_valid(rx)) {
		m_bad_crc++;
		enqueue_err_rsp(0, WB_BT_ERR_BAD_CRC);
		return;
	}

	switch (rx->type) {
	case WB_BT_TYPE_IDLE:
		break;
	case WB_BT_TYPE_PING:
		enqueue_pong();
		break;
	case WB_BT_TYPE_CMD:
		handle_cmd(rx);
		break;
	default:
		break;
	}
}

static void ble_evt_to_spi(const wb_ble_evt_t *evt)
{
	uint8_t pl[WB_BT_PAYLOAD_MAX];
	uint16_t n;

	switch (evt->type) {
	case WB_BLE_EVT_SCAN_REPORT:
		pl[0] = evt->peer.addr_type;
		memcpy(&pl[1], evt->peer.addr, BLE_GAP_ADDR_LEN);
		pl[7] = (uint8_t)evt->rssi;
		enqueue_evt(WB_BT_EVT_SCAN_REPORT, WB_BT_SENSOR_NONE, pl, 8);
		break;
	case WB_BLE_EVT_CONNECTED:
		pl[0] = evt->peer.addr_type;
		memcpy(&pl[1], evt->peer.addr, BLE_GAP_ADDR_LEN);
		enqueue_evt(WB_BT_EVT_CONNECTED, WB_BT_SENSOR_ID0, pl, 7);
		break;
	case WB_BLE_EVT_DISCONNECTED:
		pl[0] = evt->reason;
		enqueue_evt(WB_BT_EVT_DISCONNECTED, WB_BT_SENSOR_ID0, pl, 1);
		break;
	case WB_BLE_EVT_READY:
		enqueue_evt(WB_BT_EVT_READY, WB_BT_SENSOR_ID0, NULL, 0);
		break;
	case WB_BLE_EVT_DATA:
		n = evt->data_len;
		if (n > WB_BT_PAYLOAD_MAX) {
			n = WB_BT_PAYLOAD_MAX;
		}
		enqueue_data(evt->data, n);
		break;
	default:
		break;
	}
}

static void spis_event_handler(nrf_drv_spis_event_t event)
{
	if (event.evt_type == NRF_DRV_SPIS_XFER_DONE) {
		m_spis_xfer_done = true;
		m_xfer_count++;
	}
}

static void gpio_init(void)
{
	nrf_gpio_cfg_output(WB_PIN_LED);
	nrf_gpio_cfg_output(WB_PIN_READY_GP1);
	nrf_gpio_cfg_input(WB_PIN_CTRL_GP2, NRF_GPIO_PIN_NOPULL);

	WB_LED_OFF();
	WB_READY_DEASSERT();
}

static void spis_init(void)
{
	ret_code_t err;
	nrf_drv_spis_config_t cfg = NRF_DRV_SPIS_DEFAULT_CONFIG;

	cfg.miso_pin = WB_PIN_SPIS_MISO;
	cfg.mosi_pin = WB_PIN_SPIS_MOSI;
	cfg.sck_pin = WB_PIN_SPIS_SCK;
	cfg.csn_pin = WB_PIN_SPIS_CSN;
	cfg.mode = NRF_DRV_SPIS_MODE_0;
	cfg.bit_order = NRF_DRV_SPIS_BIT_ORDER_MSB_FIRST;
	cfg.orc = 0xFFu;
	cfg.def = 0xFFu;

	err = nrf_drv_spis_init(&spis, &cfg, spis_event_handler);
	APP_ERROR_CHECK(err);

	load_tx_buffer();
	err = nrf_drv_spis_buffers_set(&spis, m_tx_buf, sizeof(m_tx_buf),
				       m_rx_buf, sizeof(m_rx_buf));
	APP_ERROR_CHECK(err);
}

static void on_xfer_done(void)
{
	wb_bt_frame_t consumed;

	if (m_tx_was_queued) {
		(void)wb_txq_pop(&consumed);
	}

	process_rx_frame();
	load_tx_buffer();
	ready_from_queue();

	(void)nrf_drv_spis_buffers_set(&spis, m_tx_buf, sizeof(m_tx_buf),
				       m_rx_buf, sizeof(m_rx_buf));
}

int main(void)
{
	uint32_t blink_ms = 0;
	uint32_t status_ms = 0;
	wb_bt_frame_t hello;
	uint32_t err;

	wb_rtt_init();
	WB_RTT_PRINTF("Phase 2 S130 Central + SPI (%s)\r\n", WB_FW_ID);

	wb_txq_init();
	gpio_init();

	err = wb_ble_central_init(ble_evt_to_spi);
	APP_ERROR_CHECK(err);

	spis_init();

	wb_bt_frame_make(&hello, WB_BT_TYPE_IDLE, m_tx_seq++, 0, NULL, 0);
	(void)enqueue_frame(&hello);
	load_tx_buffer();
	(void)nrf_drv_spis_buffers_set(&spis, m_tx_buf, sizeof(m_tx_buf),
				       m_rx_buf, sizeof(m_rx_buf));
	WB_RTT_PRINTF("ready; TXQ=%u\r\n", (unsigned)wb_txq_count());

	for (;;) {
		if (m_spis_xfer_done) {
			m_spis_xfer_done = false;
			on_xfer_done();
		}

		nrf_delay_ms(1);
		if (++blink_ms >= 500u) {
			blink_ms = 0;
			WB_LED_TOGGLE();
		}
		if (++status_ms >= 5000u) {
			status_ms = 0;
			WB_RTT_PRINTF(
				"alive xfer=%lu q=%u scan=%u conn=%u seq=%u\r\n",
				(unsigned long)m_xfer_count,
				(unsigned)wb_txq_count(),
				(unsigned)wb_ble_is_scanning(),
				(unsigned)wb_ble_is_connected(),
				(unsigned)m_tx_seq);
		}
	}
}
