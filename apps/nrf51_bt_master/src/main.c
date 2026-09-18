/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Phase 0: LED + SPIS slave + GP1 ready + SEGGER RTT log (no SoftDevice).
 *
 * Build with nRF5 SDK 12.1.0 (see Makefile). SoftDevice is not required for
 * this bring-up image; flash without S130, or use a no-SD linker script.
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

static const nrf_drv_spis_t spis = NRF_DRV_SPIS_INSTANCE(1);

static uint8_t m_tx_buf[WB_BT_FRAME_SIZE];
static uint8_t m_rx_buf[WB_BT_FRAME_SIZE];
static volatile bool m_spis_xfer_done;
static uint8_t m_seq;
static uint32_t m_xfer_count;

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
		m_xfer_count++;

		/* If host sent PING, next buffer will be prepared as PONG. */
		if (event.rx_amount >= sizeof(wb_bt_frame_t)) {
			const wb_bt_frame_t *rx = (const wb_bt_frame_t *)m_rx_buf;

			if (wb_bt_frame_valid(rx) && rx->type == WB_BT_TYPE_PING) {
				WB_RTT_PRINTF("SPI: PING seq=%u -> PONG\r\n",
					      (unsigned)rx->seq);
				prepare_pong_or_idle(true);
				WB_READY_ASSERT();
			} else {
				WB_RTT_PRINTF("SPI: rx type=0x%02X len=%u\r\n",
					      (unsigned)rx->type,
					      (unsigned)event.rx_amount);
				prepare_pong_or_idle(false);
				WB_READY_DEASSERT();
			}
		} else {
			WB_RTT_PRINTF("SPI: short rx %u\r\n",
				      (unsigned)event.rx_amount);
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
	uint32_t status_ms = 0;

	wb_rtt_init();
	WB_RTT_PRINTF("Phase 0 bring-up (SPIS + LED + RTT)\r\n");
	WB_RTT_PRINTF("pins: MISO=%u MOSI=%u CSN=%u SCK=%u RDY=%u LED=%u\r\n",
		      WB_PIN_SPIS_MISO, WB_PIN_SPIS_MOSI, WB_PIN_SPIS_CSN,
		      WB_PIN_SPIS_SCK, WB_PIN_READY_GP1, WB_PIN_LED);

	gpio_init();
	spis_init();
	WB_RTT_PRINTF("SPIS ready; asserting GP1 with IDLE frame\r\n");

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
		if (++status_ms >= 5000u) {
			status_ms = 0;
			WB_RTT_PRINTF("alive xfer=%lu seq=%u\r\n",
				      (unsigned long)m_xfer_count,
				      (unsigned)m_seq);
		}
	}
}
