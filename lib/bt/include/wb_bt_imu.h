/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Shared IMU sample payload (BLE notify → master SPI DATA).
 */

#ifndef WB_BT_IMU_H
#define WB_BT_IMU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WB_BT_IMU_SAMPLE_VER 1u

/** Motion / posture classification. */
typedef enum {
	WB_MOTION_STILL     = 0,
	WB_MOTION_MOVING    = 1,
	WB_MOTION_TILT      = 2, /* |roll| or |pitch| past threshold */
	WB_MOTION_FACE_DOWN = 3,
	WB_MOTION_UPSIDE_DOWN = 4,
} wb_motion_t;

/**
 * Packed little-endian notify payload (14 bytes).
 * Angles are centi-degrees (−18000…18000). Accel is milli-g.
 */
typedef struct __attribute__((packed)) {
	uint8_t version;
	uint8_t motion;
	int16_t roll_cdeg;
	int16_t pitch_cdeg;
	int16_t yaw_cdeg;
	int16_t ax_mg;
	int16_t ay_mg;
	int16_t az_mg;
} wb_bt_imu_sample_t;

#ifdef __cplusplus
}
#endif

#endif /* WB_BT_IMU_H */
