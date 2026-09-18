/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_mpu.h"

#include "wb_twi.h"
#include "wb_sensor_board.h"
#include "wb_rtt.h"
#include "nrf_delay.h"
#include "nrf_error.h"

#define REG_SMPLRT_DIV   0x19
#define REG_CONFIG       0x1A
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_INT_PIN_CFG  0x37
#define REG_INT_ENABLE   0x38
#define REG_ACCEL_XOUT_H 0x3B
#define REG_PWR_MGMT_1   0x6B
#define REG_WHO_AM_I     0x75

static uint8_t m_addr;
static uint8_t m_whoami;

static uint32_t probe_addr(uint8_t addr)
{
	uint8_t id = 0;
	uint32_t err = wb_twi_read_reg(addr, REG_WHO_AM_I, &id, 1);

	if (err != NRF_SUCCESS) {
		return err;
	}
	/* MPU6050=0x68, MPU6500=0x70, MPU9250=0x71 */
	if (id == 0x68u || id == 0x70u || id == 0x71u || id == 0x73u) {
		m_addr = addr;
		m_whoami = id;
		return NRF_SUCCESS;
	}
	return NRF_ERROR_NOT_FOUND;
}

uint32_t wb_mpu_init(void)
{
	uint32_t err;

	err = probe_addr(WB_IMU_ADDR_PRIMARY);
	if (err != NRF_SUCCESS) {
		err = probe_addr(WB_IMU_ADDR_FALLBACK);
	}
	if (err != NRF_SUCCESS) {
		WB_RTT_PRINTF("MPU: WHO_AM_I fail\r\n");
		return err;
	}
	WB_RTT_PRINTF("MPU: addr=0x%02X whoami=0x%02X\r\n", m_addr, m_whoami);

	err = wb_twi_write_reg(m_addr, REG_PWR_MGMT_1, 0x80); /* reset */
	if (err != NRF_SUCCESS) {
		return err;
	}
	nrf_delay_ms(100);

	err = wb_twi_write_reg(m_addr, REG_PWR_MGMT_1, 0x01); /* PLL X gyro */
	if (err != NRF_SUCCESS) {
		return err;
	}
	nrf_delay_ms(10);

	err = wb_twi_write_reg(m_addr, REG_SMPLRT_DIV, 9); /* 1 kHz/(1+9)=100 Hz */
	if (err != NRF_SUCCESS) {
		return err;
	}
	err = wb_twi_write_reg(m_addr, REG_CONFIG, 0x03); /* DLPF ~44 Hz */
	if (err != NRF_SUCCESS) {
		return err;
	}
	err = wb_twi_write_reg(m_addr, REG_GYRO_CONFIG, 0x08); /* ±500 dps */
	if (err != NRF_SUCCESS) {
		return err;
	}
	err = wb_twi_write_reg(m_addr, REG_ACCEL_CONFIG, 0x08); /* ±4 g */
	if (err != NRF_SUCCESS) {
		return err;
	}
	(void)wb_twi_write_reg(m_addr, REG_INT_PIN_CFG, 0x00);
	(void)wb_twi_write_reg(m_addr, REG_INT_ENABLE, 0x00);

	return NRF_SUCCESS;
}

uint32_t wb_mpu_read(wb_mpu_raw_t *out)
{
	uint8_t buf[14];
	uint32_t err;

	if (!out) {
		return NRF_ERROR_NULL;
	}
	err = wb_twi_read_reg(m_addr, REG_ACCEL_XOUT_H, buf, sizeof(buf));
	if (err != NRF_SUCCESS) {
		return err;
	}
	out->ax = (int16_t)((buf[0] << 8) | buf[1]);
	out->ay = (int16_t)((buf[2] << 8) | buf[3]);
	out->az = (int16_t)((buf[4] << 8) | buf[5]);
	/* skip temp buf[6..7] */
	out->gx = (int16_t)((buf[8] << 8) | buf[9]);
	out->gy = (int16_t)((buf[10] << 8) | buf[11]);
	out->gz = (int16_t)((buf[12] << 8) | buf[13]);
	return NRF_SUCCESS;
}

uint8_t wb_mpu_whoami(void)
{
	return m_whoami;
}

uint8_t wb_mpu_addr(void)
{
	return m_addr;
}
