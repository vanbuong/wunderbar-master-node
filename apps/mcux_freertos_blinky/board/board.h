/*
 * Copyright 2026
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _BOARD_H_
#define _BOARD_H_

#include "clock_config.h"
#include "pin_mux.h"

#define BOARD_NAME "WunderBar Master"

#if defined(__cplusplus)
extern "C" {
#endif

static inline void BOARD_InitHardware(void)
{
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
}

#if defined(__cplusplus)
}
#endif

#endif /* _BOARD_H_ */
