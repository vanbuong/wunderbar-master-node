/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Small outbound SPI frame queue for the nRF SPI slave.
 */

#ifndef WB_TXQ_H
#define WB_TXQ_H

#include <stdbool.h>
#include <stdint.h>

#include "wb_bt_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WB_TXQ_DEPTH 4u

void wb_txq_init(void);

/** Enqueue a copy of @p f. Returns false if full. */
bool wb_txq_push(const wb_bt_frame_t *f);

/** Pop oldest frame into @p out. Returns false if empty. */
bool wb_txq_pop(wb_bt_frame_t *out);

/** Peek without removing. Returns false if empty. */
bool wb_txq_peek(wb_bt_frame_t *out);

uint8_t wb_txq_count(void);

static inline bool wb_txq_empty(void)
{
	return wb_txq_count() == 0u;
}

#ifdef __cplusplus
}
#endif

#endif /* WB_TXQ_H */
