/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Phase 1: SPIS slave with TX queue, CRC check, PING/PONG + CMD/RSP.
 * SoftDevice not required — flash alone for host-link bring-up.
 *
 * Logging: J-Link RTT Viewer / Ozone / SES Debug Terminal on BT SWD.
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_error.h"
#include "nrf_drv_spis.h"

#include "wb_nrf51_board.h"
#include "wb_rtt.h"
#include "wb_bt_frame.h"
#include "wb_txq.h"

#define WB_FW_ID "wb-nrf51-p1"

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

static void handle_get_info(void)
{
	uint8_t pl[WB_BT_PAYLOAD_MAX];
	size_t id_len = strlen(WB_FW_ID);
	size_t n;

	memset(pl, 0, sizeof(pl));
	pl[0] = (uint8_t)WB_BT_CAP_SPI; /* BLE bit set in Phase 2 */
	n = id_len;
	if (n > (sizeof(pl) - 2u)) {
		n = sizeof(pl) - 2u;
	}
	memcpy(&pl[1], WB_FW_ID, n);
	pl[1u + n] = '\0';
	enqueue_rsp(WB_BT_CMD_GET_INFO, pl, (uint16_t)(2u + n));
}

static void handle_cmd(const wb_bt_frame_t *rx)
{
	switch (rx->field_id) {
	case WB_BT_CMD_GET_INFO:
		WB_RTT_PRINTF("CMD GET_INFO\r\n");
		handle_get_info();
		break;
	case WB_BT_CMD_SCAN_START:
	case WB_BT_CMD_SCAN_STOP:
	case WB_BT_CMD_CONNECT:
	case WB_BT_CMD_DISCONNECT:
	case WB_BT_CMD_SET_SENSOR_CFG:
		/* Phase 2+ */
		WB_RTT_PRINTF("CMD 0x%02X not ready\r\n",
			      (unsigned)rx->field_id);
		enqueue_err_rsp(rx->field_id, WB_BT_ERR_BUSY);
		break;
	default:
		m_unknown_cmd++;
		WB_RTT_PRINTF("CMD unknown 0x%02X\r\n",
			      (unsigned)rx->field_id);
		enqueue_err_rsp(rx->field_id, WB_BT_ERR_UNKNOWN_CMD);
		break;
	}
}

static void process_rx_frame(void)
{
	const wb_bt_frame_t *rx = (const wb_bt_frame_t *)m_rx_buf;

	if (!wb_bt_frame_valid(rx)) {
		m_bad_crc++;
		WB_RTT_PRINTF("SPI: bad frame (crc/magic) #%lu\r\n",
			      (unsigned long)m_bad_crc);
		enqueue_err_rsp(0, WB_BT_ERR_BAD_CRC);
		return;
	}

	switch (rx->type) {
	case WB_BT_TYPE_IDLE:
		/* Nothing to do; keep duplex filled. */
		break;
	case WB_BT_TYPE_PING:
		WB_RTT_PRINTF("SPI: PING seq=%u\r\n", (unsigned)rx->seq);
		enqueue_pong();
		break;
	case WB_BT_TYPE_CMD:
		handle_cmd(rx);
		break;
	default:
		/* Unknown type: ignore (ABI). */
		WB_RTT_PRINTF("SPI: ignore type=0x%02X\r\n",
			      (unsigned)rx->type);
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

	/* Only pop if the clocked-out frame came from the TX queue. */
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

	wb_rtt_init();
	WB_RTT_PRINTF("Phase 1 SPI host protocol (%s)\r\n", WB_FW_ID);
	WB_RTT_PRINTF("pins: MISO=%u MOSI=%u CSN=%u SCK=%u RDY=%u LED=%u\r\n",
		      WB_PIN_SPIS_MISO, WB_PIN_SPIS_MOSI, WB_PIN_SPIS_CSN,
		      WB_PIN_SPIS_SCK, WB_PIN_READY_GP1, WB_PIN_LED);

	wb_txq_init();
	gpio_init();
	spis_init();

	/* Announce with one IDLE so host can detect the slave / GP1. */
	wb_bt_frame_make(&hello, WB_BT_TYPE_IDLE, m_tx_seq++, 0, NULL, 0);
	(void)enqueue_frame(&hello);
	load_tx_buffer();
	(void)nrf_drv_spis_buffers_set(&spis, m_tx_buf, sizeof(m_tx_buf),
				       m_rx_buf, sizeof(m_rx_buf));
	WB_RTT_PRINTF("SPIS ready; TXQ=%u\r\n", (unsigned)wb_txq_count());

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
				"alive xfer=%lu q=%u bad=%lu unk=%lu seq=%u\r\n",
				(unsigned long)m_xfer_count,
				(unsigned)wb_txq_count(),
				(unsigned long)m_bad_crc,
				(unsigned long)m_unknown_cmd,
				(unsigned)m_tx_seq);
		}
	}
}
