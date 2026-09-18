/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Phase 0: LED + SPIS slave + GP1 ready (no SoftDevice yet).
 *
 * Build with nRF5 SDK 12.1.0 (see Makefile). SoftDevice is not required for
 * this bring-up image; flash without S130, or use a no-SD linker script.
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
#include "wb_bt_frame.h"

#ifndef NRF_LOG_ENABLED
#define NRF_LOG_ENABLED 0
#endif

static const nrf_drv_spis_t spis = NRF_DRV_SPIS_INSTANCE(1);

static uint8_t m_tx_buf[WB_BT_FRAME_SIZE];
static uint8_t m_rx_buf[WB_BT_FRAME_SIZE];
static volatile bool m_spis_xfer_done;
static uint8_t m_seq;

static void prepare_pong_or_idle(bool pong)
{
	wb_bt_frame_t *f = (wb_bt_frame_t *)m_tx_buf;

	wb_bt_frame_init(f);
	f->seq = m_seq++;
	f->type = pong ? (uint8_t)WB_BT_TYPE_PONG : (uint8_t)WB_BT_TYPE_IDLE;
	f->field_id = 0;
	f->payload_len = 0;
	wb_bt_frame_finalize(f);
}

static void spis_event_handler(nrf_drv_spis_event_t event)
{
	if (event.evt_type == NRF_DRV_SPIS_XFER_DONE) {
		m_spis_xfer_done = true;

		/* If host sent PING, next buffer will be prepared as PONG. */
		if (event.rx_amount >= sizeof(wb_bt_frame_t)) {
			const wb_bt_frame_t *rx = (const wb_bt_frame_t *)m_rx_buf;

			if (wb_bt_frame_valid(rx) && rx->type == WB_BT_TYPE_PING) {
				prepare_pong_or_idle(true);
				WB_READY_ASSERT();
			} else {
				prepare_pong_or_idle(false);
				WB_READY_DEASSERT();
			}
		} else {
			prepare_pong_or_idle(false);
			WB_READY_DEASSERT();
		}
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

	prepare_pong_or_idle(false);
	err = nrf_drv_spis_buffers_set(&spis, m_tx_buf, sizeof(m_tx_buf),
				       m_rx_buf, sizeof(m_rx_buf));
	APP_ERROR_CHECK(err);
}

int main(void)
{
	uint32_t blink_ms = 0;

	gpio_init();
	spis_init();

	/* Announce ready with one IDLE so host can detect the slave. */
	prepare_pong_or_idle(false);
	WB_READY_ASSERT();
	(void)nrf_drv_spis_buffers_set(&spis, m_tx_buf, sizeof(m_tx_buf),
				       m_rx_buf, sizeof(m_rx_buf));

	for (;;) {
		if (m_spis_xfer_done) {
			m_spis_xfer_done = false;
			(void)nrf_drv_spis_buffers_set(&spis, m_tx_buf,
						       sizeof(m_tx_buf),
						       m_rx_buf,
						       sizeof(m_rx_buf));
		}

		nrf_delay_ms(1);
		if (++blink_ms >= 500u) {
			blink_ms = 0;
			WB_LED_TOGGLE();
		}
	}
}
