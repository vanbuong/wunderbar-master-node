/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_twi.h"

#include "nrf_drv_twi.h"
#include "nrf_error.h"
#include "app_error.h"
#include "app_util_platform.h"
#include "wb_sensor_board.h"

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);

uint32_t wb_twi_init(void)
{
	const nrf_drv_twi_config_t cfg = {
		.scl = WB_PIN_GYRO_SCL,
		.sda = WB_PIN_GYRO_SDA,
		.frequency = NRF_TWI_FREQ_100K,
		.interrupt_priority = APP_IRQ_PRIORITY_LOW,
		.clear_bus_init = false,
		.hold_bus_uninit = false,
	};

	return nrf_drv_twi_init(&m_twi, &cfg, NULL, NULL);
}

uint32_t wb_twi_write(uint8_t addr7, const uint8_t *data, uint8_t len)
{
	nrf_drv_twi_enable(&m_twi);
	return nrf_drv_twi_tx(&m_twi, addr7, data, len, false);
}

uint32_t wb_twi_write_reg(uint8_t addr7, uint8_t reg, uint8_t val)
{
	uint8_t buf[2] = { reg, val };

	nrf_drv_twi_enable(&m_twi);
	return nrf_drv_twi_tx(&m_twi, addr7, buf, sizeof(buf), false);
}

uint32_t wb_twi_read_reg(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
	uint32_t err;

	nrf_drv_twi_enable(&m_twi);
	err = nrf_drv_twi_tx(&m_twi, addr7, &reg, 1, true);
	if (err != NRF_SUCCESS) {
		return err;
	}
	return nrf_drv_twi_rx(&m_twi, addr7, buf, len);
}
