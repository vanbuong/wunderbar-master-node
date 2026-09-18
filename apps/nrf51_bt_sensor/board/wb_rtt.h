/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef WB_RTT_H
#define WB_RTT_H

#include "SEGGER_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void wb_rtt_init(void)
{
	SEGGER_RTT_ConfigUpBuffer(0, "Terminal", NULL, 0,
				  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
	SEGGER_RTT_WriteString(0, "\r\n=== wb_nrf51_bt_sensor RTT ===\r\n");
}

#define WB_RTT_PRINTF(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#define WB_RTT_WRITE(s)    SEGGER_RTT_WriteString(0, (s))

#ifdef __cplusplus
}
#endif

#endif /* WB_RTT_H */
