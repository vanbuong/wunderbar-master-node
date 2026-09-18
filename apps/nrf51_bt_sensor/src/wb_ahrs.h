/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Complementary-filter AHRS (roll/pitch) + gyro-integrated yaw.
 * Soft-float; no magnetometer — yaw drifts.
 */

#ifndef WB_AHRS_H
#define WB_AHRS_H

#include <stdint.h>
#include "wb_mpu.h"
#include "wb_bt_imu.h"

void wb_ahrs_init(void);
void wb_ahrs_update(const wb_mpu_raw_t *raw, float dt_s);
void wb_ahrs_fill_sample(wb_bt_imu_sample_t *s);

#endif /* WB_AHRS_H */
