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

#define GS_RX_RING_SIZE 256U

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

static bool s_intf_hiz;
static bool s_irq_rx;

static uint8_t s_rx_ring[GS_RX_RING_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

static void rx_ring_reset(void)
{
	unsigned int key = irq_lock();

	s_rx_head = 0U;
	s_rx_tail = 0U;
	irq_unlock(key);
}

static void rx_ring_push(uint8_t b)
{
	/*
	 * Single-producer (IRQ or poll thread): never touch tail here so the
	 * consumer can pop without racing an ISR drop-oldest update.
	 */
	uint16_t next = (uint16_t)((s_rx_head + 1U) % GS_RX_RING_SIZE);

	if (next == s_rx_tail) {
		return; /* drop newest */
	}
	s_rx_ring[s_rx_head] = b;
	s_rx_head = next;
}

static int rx_ring_pop(uint8_t *out)
{
	unsigned int key;
	uint16_t tail;

	key = irq_lock();
	tail = s_rx_tail;
	if (tail == s_rx_head) {
		irq_unlock(key);
		return 0;
	}
	*out = s_rx_ring[tail];
	s_rx_tail = (uint16_t)((tail + 1U) % GS_RX_RING_SIZE);
	irq_unlock(key);
	return 1;
}

static void zephyr_rx_poll_hw(void)
{
	unsigned char ch;

	if (!s_uart) {
		return;
	}
	while (uart_poll_in(s_uart, &ch) == 0) {
		rx_ring_push((uint8_t)ch);
	}
}

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
static void zephyr_uart_irq_handler(const struct device *dev, void *user)
{
	uint8_t ch;

	(void)user;
	if (!uart_irq_update(dev)) {
		return;
	}
	while (uart_irq_rx_ready(dev)) {
		if (uart_fifo_read(dev, &ch, 1) == 1) {
			rx_ring_push(ch);
		} else {
			break;
		}
	}
}
#endif

static int zephyr_uart_write(const uint8_t *data, size_t len, void *ctx)
{
	size_t i;

	(void)ctx;
	if (!s_uart) {
		return -1;
	}
	/*
	 * Drain RX while TX so GainSpan echo cannot overrun the HW FIFO
	 * (poll path) or starve the ring before wait_response runs.
	 */
	for (i = 0; i < len; i++) {
		uart_poll_out(s_uart, data[i]);
		if (!s_irq_rx) {
			zephyr_rx_poll_hw();
		}
	}
	if (!s_irq_rx) {
		zephyr_rx_poll_hw();
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
		if (!s_irq_rx) {
			zephyr_rx_poll_hw();
		}
		if (rx_ring_pop(&data[n]) > 0) {
			n++;
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
	(void)ctx;
	rx_ring_reset();
	zephyr_rx_poll_hw();
	rx_ring_reset();
}

static int zephyr_uart_set_baud(uint32_t baud, void *ctx)
{
	struct uart_config cfg;
	int rc;

	(void)ctx;
	if (!s_uart || baud == 0U) {
		return -1;
	}
	cfg.baudrate = baud;
	cfg.parity = UART_CFG_PARITY_NONE;
	cfg.stop_bits = UART_CFG_STOP_BITS_1;
	cfg.data_bits = UART_CFG_DATA_BITS_8;
	cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	rc = uart_configure(s_uart, &cfg);
#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
	if (rc == 0 && s_irq_rx) {
		uart_irq_rx_enable(s_uart);
	}
#endif
	return rc;
}

static uint32_t zephyr_millis(void *ctx)
{
	(void)ctx;
	return (uint32_t)k_uptime_get();
}

static void zephyr_delay_ms(uint32_t ms, void *ctx)
{
	(void)ctx;
	/*
	 * Keep draining the HW FIFO during sleeps so boot banners / echoes
	 * are not lost when IRQ RX is unavailable.
	 */
	int64_t end = k_uptime_get() + (int64_t)ms;

	while (k_uptime_get() < end) {
		if (!s_irq_rx) {
			zephyr_rx_poll_hw();
		}
		k_msleep(1);
	}
}

static void zephyr_reset_set(bool assert_reset, void *ctx)
{
	(void)ctx;
	if (!gpio_is_ready_dt(&s_reset)) {
		return;
	}
	/*
	 * Match FreeRTOS / PE BitIoLdd2: assert = drive low; release = input
	 * (board/module pull-up). Do not push-pull drive EXT_RESETn high.
	 */
	if (assert_reset) {
		(void)gpio_pin_configure_dt(&s_reset, GPIO_OUTPUT_ACTIVE);
	} else {
		(void)gpio_pin_configure(s_reset.port, s_reset.pin,
					 GPIO_INPUT | GPIO_PULL_UP);
	}
}

static void zephyr_intf_sel_set(int level, void *ctx)
{
	(void)ctx;
	if (!gpio_is_ready_dt(&s_intf)) {
		return;
	}
	if (level < 0) {
		(void)gpio_pin_configure_dt(&s_intf, GPIO_INPUT);
		s_intf_hiz = true;
		return;
	}
	(void)gpio_pin_configure(s_intf.port, s_intf.pin, GPIO_OUTPUT);
	(void)gpio_pin_set_raw(s_intf.port, s_intf.pin, level ? 1 : 0);
	s_intf_hiz = false;
}

static void zephyr_intf_sel_uart(void *ctx)
{
	zephyr_intf_sel_set((int)GS_INTF_SEL_UART_LEVEL, ctx);
}

static void zephyr_pgm_float(void)
{
	if (!gpio_is_ready_dt(&s_pgm)) {
		return;
	}
	/* PE: leave PTE6 floating — board pull keeps run mode. */
	(void)gpio_pin_configure(s_pgm.port, s_pgm.pin, GPIO_INPUT);
}

static void zephyr_pgm_level_set(uint8_t level, void *ctx)
{
	(void)ctx;
	if (!gpio_is_ready_dt(&s_pgm)) {
		return;
	}
	(void)gpio_pin_configure(s_pgm.port, s_pgm.pin, GPIO_OUTPUT);
	(void)gpio_pin_set_raw(s_pgm.port, s_pgm.pin, level ? 1 : 0);
}

static void zephyr_pgm_set(bool assert_pgm, void *ctx)
{
	if (!assert_pgm) {
		zephyr_pgm_float();
		return;
	}
	/* Assert programming: drive high across reset. */
	zephyr_pgm_level_set(1U, ctx);
}

static void zephyr_ctrl_pins_get(gs_ctrl_pins_t *out, void *ctx)
{
	(void)ctx;
	if (!out) {
		return;
	}
	memset(out, 0, sizeof(*out));
	if (gpio_is_ready_dt(&s_reset)) {
		out->reset = (uint8_t)gpio_pin_get_raw(s_reset.port, s_reset.pin);
	}
	if (gpio_is_ready_dt(&s_pgm)) {
		out->pgm = (uint8_t)gpio_pin_get_raw(s_pgm.port, s_pgm.pin);
	}
	if (gpio_is_ready_dt(&s_intf)) {
		out->intf_sel = (uint8_t)gpio_pin_get_raw(s_intf.port, s_intf.pin);
	}
	out->intf_hiz = s_intf_hiz ? 1U : 0U;
}

int gs_platform_zephyr_init(gs_platform_t *out)
{
	s_uart = DEVICE_DT_GET(GS_UART_NODE);
	if (!device_is_ready(s_uart)) {
		return -1;
	}

	rx_ring_reset();
	s_irq_rx = false;

#if defined(CONFIG_UART_INTERRUPT_DRIVEN)
	uart_irq_callback_user_data_set(s_uart, zephyr_uart_irq_handler, NULL);
	uart_irq_rx_enable(s_uart);
	s_irq_rx = true;
#endif

	if (gpio_is_ready_dt(&s_reset)) {
		/* Idle = input + pull-up (same as FreeRTOS bring-up). */
		(void)gpio_pin_configure(s_reset.port, s_reset.pin,
					 GPIO_INPUT | GPIO_PULL_UP);
	}
	if (gpio_is_ready_dt(&s_pgm)) {
		zephyr_pgm_float();
	}
	/* Legacy: leave INTF_SEL alone until bring-up tries modes. */
	zephyr_intf_sel_set(-1, NULL);

	if (out) {
		memset(out, 0, sizeof(*out));
		out->uart_write = zephyr_uart_write;
		out->uart_read = zephyr_uart_read;
		out->uart_flush = zephyr_uart_flush;
		out->uart_set_baud = zephyr_uart_set_baud;
		out->millis = zephyr_millis;
		out->delay_ms = zephyr_delay_ms;
		out->reset_set = zephyr_reset_set;
		out->intf_sel_uart = zephyr_intf_sel_uart;
		out->intf_sel_set = zephyr_intf_sel_set;
		out->pgm_set = zephyr_pgm_set;
		out->pgm_level_set = zephyr_pgm_level_set;
		out->ctrl_pins_get = zephyr_ctrl_pins_get;
		out->ctx = NULL;
		gs_platform_set(out);
	}
	return 0;
}

void gs_platform_zephyr_rx_pump(uint32_t block_ms)
{
	/*
	 * IRQ path already fills the ring. For poll UART, drain HW → ring
	 * without feeding the AT parser (command path owns that).
	 */
	int64_t end = k_uptime_get() + (int64_t)block_ms;

	if (s_irq_rx) {
		if (block_ms > 0U) {
			k_msleep(block_ms);
		}
		return;
	}

	do {
		zephyr_rx_poll_hw();
		if (block_ms == 0U) {
			break;
		}
		if (k_uptime_get() >= end) {
			break;
		}
		k_msleep(1);
	} while (k_uptime_get() < end);
}

void gs_platform_zephyr_rx_poll(uint32_t block_ms)
{
	uint8_t b;

	if (zephyr_uart_read(&b, 1, block_ms, NULL) > 0) {
		(void)gs_at_process_byte(b);
	}
}
