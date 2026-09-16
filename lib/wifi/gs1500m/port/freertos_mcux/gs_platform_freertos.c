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
/* MK64/K24: UART0_CLK_SRC is the core/system clock (see fsl_clock.h), not bus. */
#ifndef GS_UART_CLK_FREQ
#define GS_UART_CLK_FREQ (CLOCK_GetFreq(UART0_CLK_SRC))
#endif

static uint32_t gs_uart_src_hz(void)
{
	return CLOCK_GetFreq(UART0_CLK_SRC);
}

static int freertos_uart_write(const uint8_t *data, size_t len, void *ctx)
{
	(void)ctx;
	UART_WriteBlocking(GS_UART, data, len);
	return (int)len;
}

static void freertos_uart_clear_errors(void)
{
	uint32_t flags = UART_GetStatusFlags(GS_UART);
	uint32_t err = flags & (kUART_RxOverrunFlag | kUART_NoiseErrorFlag |
				kUART_FramingErrorFlag | kUART_ParityErrorFlag);
	if (err != 0U) {
		(void)UART_ClearStatusFlags(GS_UART, err);
	}
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

	while (n < max_len) {
		freertos_uart_clear_errors();
		if (UART_GetStatusFlags(GS_UART) & kUART_RxDataRegFullFlag) {
			data[n++] = UART_ReadByte(GS_UART);
			continue;
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
	freertos_uart_clear_errors();
	while (UART_GetStatusFlags(GS_UART) & kUART_RxDataRegFullFlag) {
		(void)UART_ReadByte(GS_UART);
	}
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
	(void)ctx;
	GPIO_PinWrite(GPIOD, GS_PIN_RESET_NUM, assert_reset ? 0U : 1U);
}

static void freertos_intf_sel_uart(void *ctx)
{
	(void)ctx;
	GPIO_PinWrite(GPIOA, GS_PIN_INTF_SEL_NUM, GS_INTF_SEL_UART_LEVEL);
}

static void freertos_pgm_set(bool assert_pgm, void *ctx)
{
	(void)ctx;
	/* assert_pgm true → drive opposite of idle */
	uint8_t level = assert_pgm ? (uint8_t)!GS_PGM_IDLE_LEVEL : GS_PGM_IDLE_LEVEL;
	GPIO_PinWrite(GPIOE, GS_PIN_PGM_NUM, level);
}

int gs_platform_freertos_init(gs_platform_t *out)
{
	uart_config_t uart_config;
	gpio_pin_config_t out_cfg = {
		.pinDirection = kGPIO_DigitalOutput,
		.outputLogic = 1U,
	};
	gpio_pin_config_t in_cfg = {
		.pinDirection = kGPIO_DigitalInput,
		.outputLogic = 0U,
	};

	CLOCK_EnableClock(kCLOCK_PortA);
	CLOCK_EnableClock(kCLOCK_PortB);
	CLOCK_EnableClock(kCLOCK_PortD);
	CLOCK_EnableClock(kCLOCK_PortE);
	CLOCK_EnableClock(kCLOCK_Uart0);

	/* UART0 on PTD6/PTD7 (ALT3); weak pull-up on RX when idle. */
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

	/* RESET open-drain + internal pull-up (active-low; release → high). */
	{
		const port_pin_config_t od = {
			.pullSelect = kPORT_PullUp,
			.slewRate = kPORT_FastSlewRate,
			.passiveFilterEnable = kPORT_PassiveFilterDisable,
			.openDrainEnable = kPORT_OpenDrainEnable,
			.driveStrength = kPORT_LowDriveStrength,
			.mux = kPORT_MuxAsGpio,
			.lockRegister = kPORT_UnlockRegister,
		};
		PORT_SetPinConfig(PORTD, GS_PIN_RESET_NUM, &od);
	}
	PORT_SetPinMux(PORTE, GS_PIN_PGM_NUM, kPORT_MuxAsGpio);
	PORT_SetPinMux(PORTA, GS_PIN_INTF_SEL_NUM, kPORT_MuxAsGpio);
	PORT_SetPinMux(PORTB, GS_PIN_RTC_OUT_NUM, kPORT_MuxAsGpio);
	PORT_SetPinMux(PORTD, GS_PIN_ALARM1_NUM, kPORT_MuxAsGpio);
	PORT_SetPinMux(PORTD, GS_PIN_SPI_IRQ_NUM, kPORT_MuxAsGpio);

	out_cfg.outputLogic = 1U; /* release reset (OD high-Z + pull-up) */
	GPIO_PinInit(GPIOD, GS_PIN_RESET_NUM, &out_cfg);
	out_cfg.outputLogic = GS_PGM_IDLE_LEVEL;
	GPIO_PinInit(GPIOE, GS_PIN_PGM_NUM, &out_cfg);
	/* UART mode = high (GS_INTF_SEL_UART_LEVEL); must be before reset. */
	out_cfg.outputLogic = GS_INTF_SEL_UART_LEVEL;
	GPIO_PinInit(GPIOA, GS_PIN_INTF_SEL_NUM, &out_cfg);

	GPIO_PinInit(GPIOB, GS_PIN_RTC_OUT_NUM, &in_cfg);
	GPIO_PinInit(GPIOD, GS_PIN_ALARM1_NUM, &in_cfg);
	GPIO_PinInit(GPIOD, GS_PIN_SPI_IRQ_NUM, &in_cfg);

	UART_GetDefaultConfig(&uart_config);
	uart_config.baudRate_Bps = GS_UART_BAUD_DEFAULT;
	uart_config.enableTx = true;
	uart_config.enableRx = true;
	if (UART_Init(GS_UART, &uart_config, gs_uart_src_hz()) != kStatus_Success) {
		return -1;
	}
	freertos_uart_clear_errors();


	if (out) {
		memset(out, 0, sizeof(*out));
		out->uart_write = freertos_uart_write;
		out->uart_read = freertos_uart_read;
		out->uart_flush = freertos_uart_flush;
		out->millis = freertos_millis;
		out->delay_ms = freertos_delay_ms;
		out->reset_set = freertos_reset_set;
		out->intf_sel_uart = freertos_intf_sel_uart;
		out->pgm_set = freertos_pgm_set;
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
