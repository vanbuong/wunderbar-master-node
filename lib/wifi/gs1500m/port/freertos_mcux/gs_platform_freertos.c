/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "gs_platform_freertos.h"
#include "gs1500m/pins.h"
#include "gs1500m/at.h"

#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_port.h"
#include "fsl_uart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

#ifndef GS_UART
#define GS_UART UART0
#endif

static uint32_t s_uart_baud = GS_UART_BAUD_DEFAULT;
static bool s_intf_hiz = true;
static bool s_pgm_hiz = true;

static uint32_t gs_uart_src_hz(void)
{
	/* MK64/K24: UART0_CLK_SRC is the core/system clock, not bus. */
	return CLOCK_GetFreq(UART0_CLK_SRC);
}

static int freertos_uart_write(const uint8_t *data, size_t len, void *ctx)
{
	(void)ctx;
	UART_WriteBlocking(GS_UART, data, len);
	return (int)len;
}

static int freertos_uart_read(uint8_t *data, size_t max_len, uint32_t block_ms,
			      void *ctx)
{
	size_t n = 0;
	TickType_t start = xTaskGetTickCount();
	TickType_t wait = pdMS_TO_TICKS(block_ms);
	(void)ctx;

	if (!data || max_len == 0U) {
		return 0;
	}

	/*
	 * Match MCUX UART_ReadBlocking / PE FIFO path: wait on RCFIFO count,
	 * not S1[RDRF]. With PFIFO enabled, RDRF follows the RX watermark and
	 * can miss single-byte AT replies that still sit in the FIFO.
	 */
	while (n < max_len) {
		uint32_t flags;

		if (UART_GetRxFifoCount(GS_UART) > 0U) {
			data[n++] = UART_ReadByte(GS_UART);
			continue;
		}

		flags = UART_GetStatusFlags(GS_UART);
		if ((flags & kUART_RxOverrunFlag) != 0U) {
			(void)UART_ClearStatusFlags(GS_UART, kUART_RxOverrunFlag);
		}

		if (block_ms == 0U) {
			break;
		}
		if ((xTaskGetTickCount() - start) >= wait) {
			break;
		}
		vTaskDelay(1);
	}
	return (int)n;
}

static void freertos_uart_flush(void *ctx)
{
	(void)ctx;
	while (UART_GetRxFifoCount(GS_UART) > 0U) {
		(void)UART_ReadByte(GS_UART);
	}
	if ((UART_GetStatusFlags(GS_UART) & kUART_RxOverrunFlag) != 0U) {
		(void)UART_ClearStatusFlags(GS_UART, kUART_RxOverrunFlag);
	}
}

static int freertos_uart_set_baud(uint32_t baud, void *ctx)
{
	(void)ctx;
	if (baud == 0U) {
		return -1;
	}
	if (UART_SetBaudRate(GS_UART, baud, gs_uart_src_hz()) != kStatus_Success) {
		return -1;
	}
	s_uart_baud = baud;
	return 0;
}

static uint32_t freertos_millis(void *ctx)
{
	(void)ctx;
	return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void freertos_delay_ms(uint32_t ms, void *ctx)
{
	(void)ctx;
	vTaskDelay(pdMS_TO_TICKS(ms));
}

static void freertos_reset_set(bool assert_reset, void *ctx)
{
	gpio_pin_config_t cfg;
	(void)ctx;

	/*
	 * Match PE BitIoLdd2 (PTD5): idle = GPIO input (hi-Z), PDOR preload 1.
	 * Assert = drive output low. Release = return to input (board/module pull).
	 * GainSpan EXT_RESETn is an input after POR; do not push-pull-drive high.
	 */
	if (assert_reset) {
		cfg.pinDirection = kGPIO_DigitalOutput;
		cfg.outputLogic = 0U;
		GPIO_PinInit(GPIOD, GS_PIN_RESET_NUM, &cfg);
	} else {
		GPIOD->PDOR |= (1UL << GS_PIN_RESET_NUM);
		cfg.pinDirection = kGPIO_DigitalInput;
		cfg.outputLogic = 1U;
		GPIO_PinInit(GPIOD, GS_PIN_RESET_NUM, &cfg);
	}
}

static void freertos_intf_sel_set(int level, void *ctx)
{
	gpio_pin_config_t cfg;
	(void)ctx;

	if (level < 0) {
		cfg.pinDirection = kGPIO_DigitalInput;
		cfg.outputLogic = 0U;
		GPIO_PinInit(GPIOA, GS_PIN_INTF_SEL_NUM, &cfg);
		s_intf_hiz = true;
		return;
	}

	cfg.pinDirection = kGPIO_DigitalOutput;
	cfg.outputLogic = (level != 0) ? 1U : 0U;
	GPIO_PinInit(GPIOA, GS_PIN_INTF_SEL_NUM, &cfg);
	s_intf_hiz = false;
}

static void freertos_intf_sel_uart(void *ctx)
{
	freertos_intf_sel_set((int)GS_INTF_SEL_UART_LEVEL, ctx);
}

static void freertos_pgm_level_set(uint8_t level, void *ctx)
{
	/* Legacy encoding: 0/1 drive; use pgm_set float via dedicated path. */
	gpio_pin_config_t cfg;
	(void)ctx;
	cfg.pinDirection = kGPIO_DigitalOutput;
	cfg.outputLogic = level ? 1U : 0U;
	GPIO_PinInit(GPIOE, GS_PIN_PGM_NUM, &cfg);
	s_pgm_hiz = false;
}

static void freertos_pgm_float(void)
{
	gpio_pin_config_t cfg = {
		.pinDirection = kGPIO_DigitalInput,
		.outputLogic = 0U,
	};
	/* PE has no PTE6 init — leave PGM floating (board pull = run mode). */
	GPIO_PinInit(GPIOE, GS_PIN_PGM_NUM, &cfg);
	s_pgm_hiz = true;
}

static void freertos_pgm_set(bool assert_pgm, void *ctx)
{
	(void)ctx;
	if (!assert_pgm) {
		freertos_pgm_float();
		return;
	}
	/* Assert programming: drive opposite of run-mode idle (high). */
	freertos_pgm_level_set(1U, ctx);
}

static void freertos_ctrl_pins_get(gs_ctrl_pins_t *out, void *ctx)
{
	(void)ctx;
	if (!out) {
		return;
	}
	out->reset = (uint8_t)GPIO_PinRead(GPIOD, GS_PIN_RESET_NUM);
	out->pgm = (uint8_t)GPIO_PinRead(GPIOE, GS_PIN_PGM_NUM);
	out->intf_sel = (uint8_t)GPIO_PinRead(GPIOA, GS_PIN_INTF_SEL_NUM);
	out->intf_hiz = s_intf_hiz ? 1U : 0U;
	/* Re-use unused bit: pack pgm_hiz into intf_hiz high nibble? Keep simple —
	 * pgm hi-Z is visible when we don't drive; log via pgm pin read. */
	(void)s_pgm_hiz;
}

int gs_platform_freertos_init(gs_platform_t *out)
{
	uart_config_t uart_config;
	gpio_pin_config_t in_cfg = {
		.pinDirection = kGPIO_DigitalInput,
		.outputLogic = 0U,
	};

	CLOCK_EnableClock(kCLOCK_PortA);
	CLOCK_EnableClock(kCLOCK_PortB);
	CLOCK_EnableClock(kCLOCK_PortD);
	CLOCK_EnableClock(kCLOCK_PortE);
	CLOCK_EnableClock(kCLOCK_Uart0);

	/* UART0 on PTD6/PTD7 (ALT3) — same as PE ASerialLdd1. */
	{
		const port_pin_config_t uart_rx = {
			.pullSelect = kPORT_PullUp,
			.slewRate = kPORT_FastSlewRate,
			.passiveFilterEnable = kPORT_PassiveFilterDisable,
			.openDrainEnable = kPORT_OpenDrainDisable,
			.driveStrength = kPORT_LowDriveStrength,
			.mux = kPORT_MuxAlt3,
			.lockRegister = kPORT_UnlockRegister,
		};
		const port_pin_config_t uart_tx = {
			.pullSelect = kPORT_PullDisable,
			.slewRate = kPORT_FastSlewRate,
			.passiveFilterEnable = kPORT_PassiveFilterDisable,
			.openDrainEnable = kPORT_OpenDrainDisable,
			.driveStrength = kPORT_LowDriveStrength,
			.mux = kPORT_MuxAlt3,
			.lockRegister = kPORT_UnlockRegister,
		};
		PORT_SetPinConfig(PORTD, GS_PIN_UART_RX_NUM, &uart_rx);
		PORT_SetPinConfig(PORTD, GS_PIN_UART_TX_NUM, &uart_tx);
	}

	/* PTD5 RESET — PE BitIoLdd2: GPIO input, PDOR preload high. */
	{
		const port_pin_config_t rst = {
			.pullSelect = kPORT_PullUp,
			.slewRate = kPORT_FastSlewRate,
			.passiveFilterEnable = kPORT_PassiveFilterDisable,
			.openDrainEnable = kPORT_OpenDrainDisable,
			.driveStrength = kPORT_LowDriveStrength,
			.mux = kPORT_MuxAsGpio,
			.lockRegister = kPORT_UnlockRegister,
		};
		PORT_SetPinConfig(PORTD, GS_PIN_RESET_NUM, &rst);
	}
	GPIOD->PDOR |= (1UL << GS_PIN_RESET_NUM);
	GPIO_PinInit(GPIOD, GS_PIN_RESET_NUM, &in_cfg);

	/* PTE6 PGM — PE has no init; leave as input (board pull). */
	PORT_SetPinMux(PORTE, GS_PIN_PGM_NUM, kPORT_MuxAsGpio);
	freertos_pgm_float();

	/* PTA11 INTF_SEL — PE sets MUX=1 then leaves alone; keep as input. */
	PORT_SetPinMux(PORTA, GS_PIN_INTF_SEL_NUM, kPORT_MuxAsGpio);
	freertos_intf_sel_set(-1, NULL);

	PORT_SetPinMux(PORTB, GS_PIN_RTC_OUT_NUM, kPORT_MuxAsGpio);
	PORT_SetPinMux(PORTD, GS_PIN_ALARM1_NUM, kPORT_MuxAsGpio);
	PORT_SetPinMux(PORTD, GS_PIN_SPI_IRQ_NUM, kPORT_MuxAsGpio);
	GPIO_PinInit(GPIOB, GS_PIN_RTC_OUT_NUM, &in_cfg);
	GPIO_PinInit(GPIOD, GS_PIN_ALARM1_NUM, &in_cfg);
	GPIO_PinInit(GPIOD, GS_PIN_SPI_IRQ_NUM, &in_cfg);

	UART_GetDefaultConfig(&uart_config);
	/*
	 * Match legacy PE ASerialLdd1: 115200 8N1, no modem/flow-control,
	 * RX+TX FIFO on. PE used SBR=65 BRFA=3 at SYSCLK≈120 MHz.
	 */
	uart_config.baudRate_Bps = GS_UART_BAUD_DEFAULT;
	uart_config.enableTx = true;
	uart_config.enableRx = true;
#if defined(FSL_FEATURE_UART_HAS_FIFO) && FSL_FEATURE_UART_HAS_FIFO
	uart_config.txFifoWatermark = 0;
	uart_config.rxFifoWatermark = 1;
#endif
#if defined(FSL_FEATURE_UART_HAS_MODEM_SUPPORT) && FSL_FEATURE_UART_HAS_MODEM_SUPPORT
	uart_config.enableRxRTS = false;
	uart_config.enableTxCTS = false;
#endif
	if (UART_Init(GS_UART, &uart_config, gs_uart_src_hz()) != kStatus_Success) {
		return -1;
	}
	s_uart_baud = GS_UART_BAUD_DEFAULT;
	freertos_uart_flush(NULL);

	if (out) {
		memset(out, 0, sizeof(*out));
		out->uart_write = freertos_uart_write;
		out->uart_read = freertos_uart_read;
		out->uart_flush = freertos_uart_flush;
		out->uart_set_baud = freertos_uart_set_baud;
		out->millis = freertos_millis;
		out->delay_ms = freertos_delay_ms;
		out->reset_set = freertos_reset_set;
		out->intf_sel_uart = freertos_intf_sel_uart;
		out->intf_sel_set = freertos_intf_sel_set;
		out->pgm_set = freertos_pgm_set;
		out->pgm_level_set = freertos_pgm_level_set;
		out->ctrl_pins_get = freertos_ctrl_pins_get;
		out->ctx = NULL;
		gs_platform_set(out);
	}
	return 0;
}

void gs_platform_freertos_rx_poll(uint32_t block_ms)
{
	uint8_t b;
	if (freertos_uart_read(&b, 1, block_ms, NULL) > 0) {
		(void)gs_at_process_byte(b);
	}
}
