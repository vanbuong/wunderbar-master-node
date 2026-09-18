/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Minimal MPU-6050 / MPU-6500 driver (accel + gyro).
 */

#ifndef WB_MPU_H
#define WB_MPU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int16_t ax, ay, az; /* raw */
	int16_t gx, gy, gz;
} wb_mpu_raw_t;

uint32_t wb_mpu_init(void);
uint32_t wb_mpu_read(wb_mpu_raw_t *out);
uint8_t wb_mpu_whoami(void);
uint8_t wb_mpu_addr(void);

#endif /* WB_MPU_H */
