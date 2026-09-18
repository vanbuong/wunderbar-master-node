/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * Phase 1 host stub: SPI master toward nRF51822 (WBBT frames).
 * Exercises PING/PONG and CMD GET_INFO; watches bt-ready (GP1).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

#include <errno.h>
#include <string.h>

#include "wb_bt_frame.h"

LOG_MODULE_REGISTER(wb_bt_host, LOG_LEVEL_INF);

#if DT_HAS_CHOSEN(zephyr_console) && \
	DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart)
#define CONSOLE_IS_USB_CDC 1
#include <zephyr/drivers/uart.h>
#else
#define CONSOLE_IS_USB_CDC 0
#endif

#define DTR_WAIT_MS 5000
#define BT_READY_NODE DT_ALIAS(bt_ready)
#define BT_SPI_NODE DT_NODELABEL(bt_nrf51822_spi)

#if !DT_NODE_EXISTS(BT_READY_NODE)
#error "Board must define alias bt-ready (PTA10 / GP1)"
#endif

#if !DT_NODE_EXISTS(BT_SPI_NODE)
#error "Board must define bt_nrf51822_spi on SPI0"
#endif

static const struct gpio_dt_spec bt_ready =
	GPIO_DT_SPEC_GET(BT_READY_NODE, gpios);

static const struct spi_dt_spec bt_spi =
	SPI_DT_SPEC_GET(BT_SPI_NODE,
			SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB);

static uint8_t m_host_seq;

#if CONSOLE_IS_USB_CDC
static void wait_for_dtr(void)
{
	const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;
	int waited = 0;

	if (!device_is_ready(cons)) {
		return;
	}

	while (!dtr && waited < DTR_WAIT_MS) {
		(void)uart_line_ctrl_get(cons, UART_LINE_CTRL_DTR, &dtr);
		k_msleep(100);
		waited += 100;
	}
}
#endif

static int bt_xfer(const wb_bt_frame_t *tx, wb_bt_frame_t *rx)
{
	const struct spi_buf tx_bufs[] = {
		{ .buf = (void *)tx, .len = sizeof(*tx) },
	};
	const struct spi_buf rx_bufs[] = {
		{ .buf = rx, .len = sizeof(*rx) },
	};
	const struct spi_buf_set tx_set = {
		.buffers = tx_bufs,
		.count = ARRAY_SIZE(tx_bufs),
	};
	const struct spi_buf_set rx_set = {
		.buffers = rx_bufs,
		.count = ARRAY_SIZE(rx_bufs),
	};

	return spi_transceive_dt(&bt_spi, &tx_set, &rx_set);
}

static int bt_xfer_idle(wb_bt_frame_t *rx)
{
	wb_bt_frame_t tx;

	wb_bt_frame_make(&tx, WB_BT_TYPE_IDLE, m_host_seq++, 0, NULL, 0);
	return bt_xfer(&tx, rx);
}

static void log_frame(const char *dir, const wb_bt_frame_t *f)
{
	if (!wb_bt_frame_valid(f)) {
		LOG_WRN("%s invalid frame (crc/magic)", dir);
		return;
	}
	LOG_INF("%s type=0x%02X seq=%u field=0x%02X len=%u", dir,
		f->type, f->seq, f->field_id, f->payload_len);
}

/**
 * Full duplex: MOSI request is handled after the clock finishes, so the
 * reply appears on a later transaction (often after GP1 asserts).
 */
static int drain_until(uint8_t want_type, uint8_t want_field, wb_bt_frame_t *out,
		       int max_tries)
{
	wb_bt_frame_t rx;
	int ret;
	int i;

	for (i = 0; i < max_tries; i++) {
		ret = bt_xfer_idle(&rx);
		if (ret < 0) {
			return ret;
		}
		log_frame("RX", &rx);
		if (wb_bt_frame_valid(&rx) && rx.type == want_type &&
		    (want_field == 0xFFu || rx.field_id == want_field)) {
			if (out) {
				*out = rx;
			}
			return 0;
		}
		/* Brief pause so nRF main loop can enqueue. */
		k_msleep(2);
	}
	return -ETIMEDOUT;
}

static int do_ping(void)
{
	wb_bt_frame_t tx;
	wb_bt_frame_t rx;
	int ret;

	wb_bt_frame_make(&tx, WB_BT_TYPE_PING, m_host_seq++, 0, NULL, 0);
	ret = bt_xfer(&tx, &rx);
	if (ret < 0) {
		LOG_ERR("PING xfer failed (%d)", ret);
		return ret;
	}
	log_frame("TX", &tx);
	log_frame("RX", &rx);

	if (wb_bt_frame_valid(&rx) && rx.type == WB_BT_TYPE_PONG) {
		LOG_INF("PING -> PONG ok (same xfer)");
		return 0;
	}

	ret = drain_until(WB_BT_TYPE_PONG, 0xFFu, &rx, 6);
	if (ret < 0) {
		LOG_WRN("expected PONG (%d)", ret);
		return ret;
	}
	LOG_INF("PING -> PONG ok");
	return 0;
}

static int do_get_info(void)
{
	wb_bt_frame_t tx;
	wb_bt_frame_t rx;
	int ret;
	char id[WB_BT_PAYLOAD_MAX];

	wb_bt_frame_make(&tx, WB_BT_TYPE_CMD, m_host_seq++,
			 WB_BT_CMD_GET_INFO, NULL, 0);
	ret = bt_xfer(&tx, &rx);
	if (ret < 0) {
		LOG_ERR("GET_INFO xfer failed (%d)", ret);
		return ret;
	}
	log_frame("TX", &tx);
	log_frame("RX", &rx);

	if (!(wb_bt_frame_valid(&rx) && rx.type == WB_BT_TYPE_RSP &&
	      rx.field_id == WB_BT_CMD_GET_INFO && rx.payload_len >= 2)) {
		ret = drain_until(WB_BT_TYPE_RSP, WB_BT_CMD_GET_INFO, &rx, 6);
		if (ret < 0) {
			LOG_WRN("GET_INFO: no RSP");
			return ret;
		}
	}

	memset(id, 0, sizeof(id));
	memcpy(id, &rx.payload[1],
	       rx.payload_len > 1 ? rx.payload_len - 1u : 0);
	LOG_INF("GET_INFO caps=0x%02X id=\"%s\"", rx.payload[0], id);
	return 0;
}

int main(void)
{
	int ret;
	int ready;

#if CONSOLE_IS_USB_CDC
	wait_for_dtr();
	LOG_INF("WunderBar BT host stub (USB)");
#else
	LOG_INF("WunderBar BT host stub (RTT)");
#endif

	if (!gpio_is_ready_dt(&bt_ready)) {
		LOG_ERR("bt-ready GPIO not ready");
		return 0;
	}
	ret = gpio_pin_configure_dt(&bt_ready, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("bt-ready configure failed (%d)", ret);
		return 0;
	}

	if (!spi_is_ready_dt(&bt_spi)) {
		LOG_ERR("BT SPI not ready");
		return 0;
	}

	ready = gpio_pin_get_dt(&bt_ready);
	LOG_INF("bt-ready(GP1)=%d  spi@1MHz mode0", ready);

	/* Give nRF a moment after reset if both boards boot together. */
	k_msleep(200);

	(void)do_ping();
	k_msleep(50);
	(void)do_get_info();

	while (1) {
		ready = gpio_pin_get_dt(&bt_ready);
		LOG_INF("poll: ready=%d — PING", ready);
		(void)do_ping();
		k_msleep(2000);
	}

	return 0;
}
