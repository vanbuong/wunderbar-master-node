/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Thin SEGGER RTT logging for the nRF51822 BT master (J-Link SWD).
 */

#ifndef WB_RTT_H
#define WB_RTT_H

#include "SEGGER_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void wb_rtt_init(void)
{
	/*
	 * Up-buffer 0: non-blocking skip if host is not connected so SPI/BLE
	 * timing is not stalled by a full RTT buffer.
	 */
	SEGGER_RTT_ConfigUpBuffer(0, "Terminal", NULL, 0,
				  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
	SEGGER_RTT_WriteString(0, "\r\n=== wb_nrf51_bt_master RTT ===\r\n");
}

#define WB_RTT_PRINTF(...) SEGGER_RTT_printf(0, __VA_ARGS__)
#define WB_RTT_WRITE(s)    SEGGER_RTT_WriteString(0, (s))

#ifdef __cplusplus
}
#endif

#endif /* WB_RTT_H */
