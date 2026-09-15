/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _CLOCK_CONFIG_H_
#define _CLOCK_CONFIG_H_

#include "fsl_common.h"

/*******************************************************************************
 * Board crystals (WunderBar master schematic)
 ******************************************************************************/
#define BOARD_XTAL0_CLK_HZ   12000000U /*!< 12 MHz system crystal (Y1) */
#define BOARD_XTAL32K_CLK_HZ 32768U    /*!< 32.768 kHz RTC crystal (X8) */

#if defined(__cplusplus)
extern "C" {
#endif

void BOARD_InitBootClocks(void);

/*******************************************************************************
 * BOARD_BootClockRUN — 120 MHz PEE from 12 MHz crystal
 ******************************************************************************/
#define BOARD_BOOTCLOCKRUN_CORE_CLOCK 120000000U

extern const mcg_config_t mcgConfig_BOARD_BootClockRUN;
extern const sim_clock_config_t simConfig_BOARD_BootClockRUN;
extern const osc_config_t oscConfig_BOARD_BootClockRUN;

void BOARD_BootClockRUN(void);

/*******************************************************************************
 * BOARD_BootClockVLPR — 4 MHz BLPI
 ******************************************************************************/
#define BOARD_BOOTCLOCKVLPR_CORE_CLOCK 4000000U

extern const mcg_config_t mcgConfig_BOARD_BootClockVLPR;
extern const sim_clock_config_t simConfig_BOARD_BootClockVLPR;
extern const osc_config_t oscConfig_BOARD_BootClockVLPR;

void BOARD_BootClockVLPR(void);

#if defined(__cplusplus)
}
#endif

#endif /* _CLOCK_CONFIG_H_ */
