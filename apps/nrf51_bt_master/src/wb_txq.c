/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_txq.h"

#include <string.h>

static wb_bt_frame_t m_q[WB_TXQ_DEPTH];
static uint8_t m_head;
static uint8_t m_tail;
static uint8_t m_count;

void wb_txq_init(void)
{
	m_head = 0;
	m_tail = 0;
	m_count = 0;
}

bool wb_txq_push(const wb_bt_frame_t *f)
{
	if (!f || m_count >= WB_TXQ_DEPTH) {
		return false;
	}
	memcpy(&m_q[m_tail], f, sizeof(*f));
	m_tail = (uint8_t)((m_tail + 1u) % WB_TXQ_DEPTH);
	m_count++;
	return true;
}

bool wb_txq_pop(wb_bt_frame_t *out)
{
	if (!out || m_count == 0u) {
		return false;
	}
	memcpy(out, &m_q[m_head], sizeof(*out));
	m_head = (uint8_t)((m_head + 1u) % WB_TXQ_DEPTH);
	m_count--;
	return true;
}

bool wb_txq_peek(wb_bt_frame_t *out)
{
	if (!out || m_count == 0u) {
		return false;
	}
	memcpy(out, &m_q[m_head], sizeof(*out));
	return true;
}

uint8_t wb_txq_count(void)
{
	return m_count;
}
