/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs_platform_zephyr.h"
#include "gs1500m/pins.h"
#include "gs1500m/at.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#include <string.h>

#define GS_UART_NODE DT_NODELABEL(uart0)

#if !DT_NODE_HAS_STATUS(GS_UART_NODE, okay)
#error "uart0 must be enabled in board DTS for GS1500M"
#endif

static const struct device *s_uart;
static const struct gpio_dt_spec s_reset =
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(wifi_reset), gpios, { 0 });
static const struct gpio_dt_spec s_pgm =
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(wifi_pgm), gpios, { 0 });
static const struct gpio_dt_spec s_intf =
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(wifi_intf_sel), gpios, { 0 });

#ifndef GS_INTF_SEL_UART_LEVEL
#define GS_INTF_SEL_UART_LEVEL 1U
#endif

static int zephyr_uart_write(const uint8_t *data, size_t len, void *ctx)
{
	size_t i;
	(void)ctx;
	if (!s_uart) {
		return -1;
	}
	for (i = 0; i < len; i++) {
		uart_poll_out(s_uart, data[i]);
	}
	return (int)len;
}

static int zephyr_uart_read(uint8_t *data, size_t max_len, uint32_t block_ms,
			    void *ctx)
{
	size_t n = 0;
	int64_t end = k_uptime_get() + (int64_t)block_ms;
	(void)ctx;

	if (!s_uart || !data || max_len == 0U) {
		return 0;
	}

	while (n < max_len) {
		unsigned char ch;
		int rc = uart_poll_in(s_uart, &ch);
		if (rc == 0) {
			data[n++] = (uint8_t)ch;
			continue;
		}
		if (block_ms == 0U) {
			break;
		}
		if (k_uptime_get() >= end) {
			break;
		}
		k_msleep(1);
	}
	return (int)n;
}

static void zephyr_uart_flush(void *ctx)
{
	unsigned char ch;
	(void)ctx;
	if (!s_uart) {
		return;
	}
	while (uart_poll_in(s_uart, &ch) == 0) {
	}
}

static uint32_t zephyr_millis(void *ctx)
{
	(void)ctx;
	return (uint32_t)k_uptime_get();
}

static void zephyr_delay_ms(uint32_t ms, void *ctx)
{
	(void)ctx;
	k_msleep(ms);
}

static void zephyr_reset_set(bool assert_reset, void *ctx)
{
	(void)ctx;
	if (!gpio_is_ready_dt(&s_reset)) {
		return;
	}
	/* ACTIVE_LOW in DTS: logical 1 = assert reset */
	(void)gpio_pin_set_dt(&s_reset, assert_reset ? 1 : 0);
}

static void zephyr_intf_sel_uart(void *ctx)
{
	(void)ctx;
	if (!gpio_is_ready_dt(&s_intf)) {
		return;
	}
	(void)gpio_pin_set_dt(&s_intf, (int)GS_INTF_SEL_UART_LEVEL);
}

static void zephyr_pgm_set(bool assert_pgm, void *ctx)
{
	(void)ctx;
	if (!gpio_is_ready_dt(&s_pgm)) {
		return;
	}
	/* Keep idle when not asserting programming mode. */
	(void)gpio_pin_set_dt(&s_pgm, assert_pgm ? 1 : 0);
}

int gs_platform_zephyr_init(gs_platform_t *out)
{
	s_uart = DEVICE_DT_GET(GS_UART_NODE);
	if (!device_is_ready(s_uart)) {
		return -1;
	}

	if (gpio_is_ready_dt(&s_reset)) {
		(void)gpio_pin_configure_dt(&s_reset, GPIO_OUTPUT_INACTIVE);
	}
	if (gpio_is_ready_dt(&s_pgm)) {
		(void)gpio_pin_configure(s_pgm.port, s_pgm.pin, GPIO_OUTPUT);
		(void)gpio_pin_set_raw(s_pgm.port, s_pgm.pin, (int)GS_PGM_IDLE_LEVEL);
	}
	if (gpio_is_ready_dt(&s_intf)) {
		(void)gpio_pin_configure(s_intf.port, s_intf.pin, GPIO_OUTPUT);
		(void)gpio_pin_set_raw(s_intf.port, s_intf.pin, (int)GS_INTF_SEL_UART_LEVEL);
	}

	if (out) {
		memset(out, 0, sizeof(*out));
		out->uart_write = zephyr_uart_write;
		out->uart_read = zephyr_uart_read;
		out->uart_flush = zephyr_uart_flush;
		out->millis = zephyr_millis;
		out->delay_ms = zephyr_delay_ms;
		out->reset_set = zephyr_reset_set;
		out->intf_sel_uart = zephyr_intf_sel_uart;
		out->pgm_set = zephyr_pgm_set;
		out->ctx = NULL;
		gs_platform_set(out);
	}
	return 0;
}

void gs_platform_zephyr_rx_poll(uint32_t block_ms)
{
	uint8_t b;
	if (zephyr_uart_read(&b, 1, block_ms, NULL) > 0) {
		(void)gs_at_process_byte(b);
	}
}
