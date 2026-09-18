/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_ahrs.h"

#include <math.h>

/* ±4 g → LSB/g = 8192; ±500 dps → LSB/(°/s) = 65.5 */
#define ACC_LSB_PER_G   8192.0f
#define GYRO_LSB_PER_DPS 65.5f
#define ALPHA           0.98f
#define TILT_DEG        30.0f
#define STILL_GYRO_DPS  3.0f
#define STILL_ACC_MG    80

static float m_roll_deg;
static float m_pitch_deg;
static float m_yaw_deg;
static float m_ax_g, m_ay_g, m_az_g;
static float m_gx_dps, m_gy_dps, m_gz_dps;
static uint8_t m_still_count;

void wb_ahrs_init(void)
{
	m_roll_deg = 0.f;
	m_pitch_deg = 0.f;
	m_yaw_deg = 0.f;
	m_still_count = 0;
}

static float rad2deg(float r)
{
	return r * (180.0f / 3.14159265f);
}

void wb_ahrs_update(const wb_mpu_raw_t *raw, float dt_s)
{
	float ax, ay, az;
	float gx, gy, gz;
	float roll_acc, pitch_acc;
	float gyro_mag;

	if (!raw || dt_s <= 0.f) {
		return;
	}

	ax = (float)raw->ax / ACC_LSB_PER_G;
	ay = (float)raw->ay / ACC_LSB_PER_G;
	az = (float)raw->az / ACC_LSB_PER_G;
	gx = (float)raw->gx / GYRO_LSB_PER_DPS;
	gy = (float)raw->gy / GYRO_LSB_PER_DPS;
	gz = (float)raw->gz / GYRO_LSB_PER_DPS;

	m_ax_g = ax;
	m_ay_g = ay;
	m_az_g = az;
	m_gx_dps = gx;
	m_gy_dps = gy;
	m_gz_dps = gz;

	roll_acc = rad2deg(atan2f(ay, az));
	pitch_acc = rad2deg(atan2f(-ax, sqrtf(ay * ay + az * az)));

	m_roll_deg = ALPHA * (m_roll_deg + gx * dt_s) + (1.f - ALPHA) * roll_acc;
	m_pitch_deg =
		ALPHA * (m_pitch_deg + gy * dt_s) + (1.f - ALPHA) * pitch_acc;
	m_yaw_deg += gz * dt_s;
	if (m_yaw_deg > 180.f) {
		m_yaw_deg -= 360.f;
	} else if (m_yaw_deg < -180.f) {
		m_yaw_deg += 360.f;
	}

	gyro_mag = fabsf(gx) + fabsf(gy) + fabsf(gz);
	if (gyro_mag < STILL_GYRO_DPS) {
		if (m_still_count < 255u) {
			m_still_count++;
		}
	} else {
		m_still_count = 0;
	}
}

static int16_t to_cdeg(float deg)
{
	float v = deg * 100.f;

	if (v > 32767.f) {
		v = 32767.f;
	}
	if (v < -32768.f) {
		v = -32768.f;
	}
	return (int16_t)v;
}

static int16_t to_mg(float g)
{
	float v = g * 1000.f;

	if (v > 32767.f) {
		v = 32767.f;
	}
	if (v < -32768.f) {
		v = -32768.f;
	}
	return (int16_t)v;
}

static uint8_t classify_motion(void)
{
	float ar = fabsf(m_roll_deg);
	float ap = fabsf(m_pitch_deg);
	float a_norm = sqrtf(m_ax_g * m_ax_g + m_ay_g * m_ay_g + m_az_g * m_az_g);
	int accel_delta_mg = (int)(fabsf(a_norm - 1.f) * 1000.f);

	if (m_az_g < -0.7f && ar < 45.f && ap < 45.f) {
		return WB_MOTION_FACE_DOWN;
	}
	if (m_az_g < 0.f && (ar > 135.f || ap > 135.f)) {
		return WB_MOTION_UPSIDE_DOWN;
	}
	if (ar > TILT_DEG || ap > TILT_DEG) {
		return WB_MOTION_TILT;
	}
	if (m_still_count > 20u && accel_delta_mg < STILL_ACC_MG) {
		return WB_MOTION_STILL;
	}
	return WB_MOTION_MOVING;
}

void wb_ahrs_fill_sample(wb_bt_imu_sample_t *s)
{
	if (!s) {
		return;
	}
	s->version = WB_BT_IMU_SAMPLE_VER;
	s->motion = classify_motion();
	s->roll_cdeg = to_cdeg(m_roll_deg);
	s->pitch_cdeg = to_cdeg(m_pitch_deg);
	s->yaw_cdeg = to_cdeg(m_yaw_deg);
	s->ax_mg = to_mg(m_ax_g);
	s->ay_mg = to_mg(m_ay_g);
	s->az_mg = to_mg(m_az_g);
}
