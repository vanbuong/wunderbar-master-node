/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Line FSM + ESC S/Z/Y/u/H/O/F parser (behavior from GainSpan AtCmdLib RX path).
 */

#include "gs1500m/at.h"
#include "gs1500m/platform.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef enum {
	RX_START = 0,
	RX_LINE,
	RX_ESC,
	RX_STREAM_CID,
	RX_STREAM_DATA,
	RX_BULK_CID,
	RX_BULK_LEN,
	RX_BULK_DATA,
	RX_HTTP_CID,
	RX_HTTP_LEN,
	RX_HTTP_DATA,
	/* ESC Y: CID + IP + ' ' + PORT + '\t' + 4-digit len + data */
	RX_UDP_BULK_CID,
	RX_UDP_BULK_IP,
	RX_UDP_BULK_PORT,
	RX_UDP_BULK_LEN,
	RX_UDP_BULK_DATA,
	/* ESC u: CID + IP + ' ' + PORT + '\t' + data + ESC E */
	RX_UDP_CID,
	RX_UDP_IP,
	RX_UDP_PORT,
	RX_UDP_DATA,
} rx_state_t;

static gs_at_callbacks_t s_cbs;
static rx_state_t s_state;
static char s_line[GS_AT_RX_LINE_MAX];
static size_t s_line_len;
static char s_last_line[GS_AT_RX_LINE_MAX];
static char s_last_info_line[GS_AT_RX_LINE_MAX];
static char s_info_accum[GS_AT_RX_LINE_MAX];
static uint32_t s_rx_bytes;
static uint8_t s_cid;
static gs_esc_kind_t s_esc_kind;
static uint32_t s_bulk_len;
static uint32_t s_bulk_got;
static char s_len_digits[GS_AT_BULK_LEN_DIGITS];
static uint8_t s_len_idx;
static uint8_t s_data_chunk[64];
static size_t s_data_chunk_len;
static bool s_stream_esc_pending;

static void emit_data_flush(void)
{
	if (s_data_chunk_len == 0U) {
		return;
	}
	if (s_cbs.on_data) {
		s_cbs.on_data(s_cid, s_esc_kind, s_data_chunk, s_data_chunk_len,
			      s_cbs.user);
	}
	s_data_chunk_len = 0U;
}

static void emit_data_byte(uint8_t b)
{
	s_data_chunk[s_data_chunk_len++] = b;
	if (s_data_chunk_len >= sizeof(s_data_chunk)) {
		emit_data_flush();
	}
}

static void reset_to_start(void)
{
	emit_data_flush();
	s_state = RX_START;
	s_line_len = 0U;
	s_bulk_len = 0U;
	s_bulk_got = 0U;
	s_len_idx = 0U;
	s_stream_esc_pending = false;
	s_esc_kind = GS_ESC_KIND_NONE;
}

static void clear_info_accum(void)
{
	s_info_accum[0] = '\0';
}

static void append_info_accum(const char *line)
{
	size_t cur;
	size_t add;
	size_t room;

	if (!line || !line[0]) {
		return;
	}
	cur = strlen(s_info_accum);
	add = strlen(line);
	room = sizeof(s_info_accum) - 1U;
	if (cur > 0U && cur < room) {
		s_info_accum[cur++] = '\n';
		s_info_accum[cur] = '\0';
	}
	if (cur >= room) {
		return;
	}
	if (add > room - cur) {
		add = room - cur;
	}
	memcpy(s_info_accum + cur, line, add);
	s_info_accum[cur + add] = '\0';
}

void gs_at_init(const gs_at_callbacks_t *cbs)
{
	memset(&s_cbs, 0, sizeof(s_cbs));
	if (cbs) {
		s_cbs = *cbs;
	}
	memset(s_line, 0, sizeof(s_line));
	memset(s_last_line, 0, sizeof(s_last_line));
	memset(s_last_info_line, 0, sizeof(s_last_info_line));
	clear_info_accum();
	s_rx_bytes = 0U;
	reset_to_start();
}

void gs_at_set_callbacks(const gs_at_callbacks_t *cbs)
{
	if (cbs) {
		s_cbs = *cbs;
	} else {
		memset(&s_cbs, 0, sizeof(s_cbs));
	}
}

static bool token_at_start(const char *line, const char *tok)
{
	size_t n;

	if (!line || !tok) {
		return false;
	}
	n = strlen(tok);
	if (strncmp(line, tok, n) != 0) {
		return false;
	}
	return line[n] == '\0' || line[n] == '\r' || line[n] == '\n' ||
	       line[n] == ' ' || line[n] == '\t';
}

gs_msg_id_t gs_at_classify_line(const char *line)
{
	const char *s = line;

	if (!s) {
		return GS_MSG_NONE;
	}
	while (*s == ' ' || *s == '\t') {
		s++;
	}

	if (strstr(s, "ERROR: INVALID INPUT")) {
		return GS_MSG_INVALID_INPUT;
	}
	if (strstr(s, "ERROR: IP CONFIG FAIL")) {
		return GS_MSG_ERROR_IP_CONFIG;
	}
	if (strstr(s, "ERROR: SOCKET FAILURE")) {
		return GS_MSG_ERROR_SOCKET;
	}
	/* Require ERROR at start so status lines cannot false-trigger. */
	if (token_at_start(s, "ERROR")) {
		return GS_MSG_ERROR;
	}
	/* Exact "OK" after leading whitespace (GainSpan pads some dumps). */
	if (token_at_start(s, "OK")) {
		return GS_MSG_OK;
	}
	if (strstr(s, "DISASSOCIATED") || strstr(s, "Disassociation Event")) {
		return GS_MSG_DISASSOCIATED;
	}
	if (token_at_start(s, "DISCONNECT") || strstr(s, "DISCONNECT")) {
		return GS_MSG_DISCONNECT;
	}
	if (strstr(s, "APP Reset") || strstr(s, "UnExpected Warm Boot")) {
		return GS_MSG_APP_RESET;
	}
	if (strstr(s, "Serial2WiFi APP")) {
		return GS_MSG_WELCOME;
	}
	if (strstr(s, "External Flash FW-UP-SUCCESS")) {
		return GS_MSG_FW_UPDATE_OK;
	}
	if (token_at_start(s, "CONNECT")) {
		/* Server accept lines are long: CONNECT <srv> <cli> <ip> <port> */
		if (strlen(s) > 20U) {
			return GS_MSG_CONNECT_SERVER_CLIENT;
		}
		return GS_MSG_CONNECT;
	}
	return GS_MSG_NONE;
}

static gs_msg_id_t finish_line(void)
{
	gs_msg_id_t id;

	if (s_line_len >= sizeof(s_line)) {
		s_line_len = sizeof(s_line) - 1U;
	}
	s_line[s_line_len] = '\0';
	strncpy(s_last_line, s_line, sizeof(s_last_line) - 1U);
	s_last_line[sizeof(s_last_line) - 1U] = '\0';

	id = gs_at_classify_line(s_line);
	if ((id == GS_MSG_NONE || id == GS_MSG_WELCOME || id == GS_MSG_APP_RESET) &&
	    s_line_len > 0U) {
		strncpy(s_last_info_line, s_line, sizeof(s_last_info_line) - 1U);
		s_last_info_line[sizeof(s_last_info_line) - 1U] = '\0';
		{
			size_t n = strlen(s_last_info_line);
			while (n > 0U && (s_last_info_line[n - 1U] == '\r' ||
					  s_last_info_line[n - 1U] == '\n')) {
				s_last_info_line[--n] = '\0';
			}
		}
		append_info_accum(s_last_info_line);
	}
	if (s_cbs.on_line && (id != GS_MSG_NONE || s_line_len > 0U)) {
		s_cbs.on_line(id, s_line, s_cbs.user);
	}
	s_line_len = 0U;
	s_state = RX_START;
	return id;
}

gs_msg_id_t gs_at_process_byte(uint8_t b)
{
	gs_msg_id_t id = GS_MSG_NONE;

	s_rx_bytes++;

	switch (s_state) {
	case RX_START:
		if (b == '\r' || b == '\n' || b == 0U) {
			break;
		}
		if (b == GS_AT_ESC) {
			s_state = RX_ESC;
			break;
		}
		s_line_len = 0U;
		s_line[s_line_len++] = (char)b;
		s_state = RX_LINE;
		break;

	case RX_LINE:
		if (b == GS_AT_ESC) {
			s_state = RX_ESC;
			break;
		}
		if (b == '\r' || b == '\n') {
			id = finish_line();
			break;
		}
		if (s_line_len + 1U < sizeof(s_line)) {
			s_line[s_line_len++] = (char)b;
		}
		break;

	case RX_ESC:
		switch (b) {
		case 'Z':
			s_esc_kind = GS_ESC_KIND_BULK;
			s_state = RX_BULK_CID;
			s_len_idx = 0U;
			s_bulk_len = 0U;
			s_bulk_got = 0U;
			break;
		case 'H':
			s_esc_kind = GS_ESC_KIND_HTTP;
			s_state = RX_HTTP_CID;
			s_len_idx = 0U;
			s_bulk_len = 0U;
			s_bulk_got = 0U;
			break;
		case 'S':
			s_esc_kind = GS_ESC_KIND_STREAM;
			s_state = RX_STREAM_CID;
			s_stream_esc_pending = false;
			break;
		case 'Y':
			s_esc_kind = GS_ESC_KIND_UDP_BULK;
			s_state = RX_UDP_BULK_CID;
			s_len_idx = 0U;
			s_bulk_len = 0U;
			s_bulk_got = 0U;
			break;
		case 'u':
		case 'U':
			s_esc_kind = GS_ESC_KIND_UDP;
			s_state = RX_UDP_CID;
			s_stream_esc_pending = false;
			break;
		case 'O':
			reset_to_start();
			id = GS_MSG_ESC_OK;
			break;
		case 'F':
			reset_to_start();
			id = GS_MSG_ESC_FAIL;
			break;
		default:
			reset_to_start();
			break;
		}
		break;

	case RX_STREAM_CID:
		s_cid = gs_at_ascii_to_cid(b);
		s_state = RX_STREAM_DATA;
		break;

	case RX_STREAM_DATA:
		if (s_stream_esc_pending) {
			s_stream_esc_pending = false;
			if (b == 'E') {
				emit_data_flush();
				id = GS_MSG_STREAM_DATA;
				reset_to_start();
			} else {
				emit_data_byte(GS_AT_ESC);
				emit_data_byte(b);
			}
			break;
		}
		if (b == GS_AT_ESC) {
			s_stream_esc_pending = true;
			break;
		}
		emit_data_byte(b);
		break;

	case RX_BULK_CID:
		s_cid = gs_at_ascii_to_cid(b);
		s_state = RX_BULK_LEN;
		s_len_idx = 0U;
		s_bulk_len = 0U;
		break;

	case RX_BULK_LEN:
		if (b < '0' || b > '9') {
			reset_to_start();
			break;
		}
		s_len_digits[s_len_idx++] = (char)b;
		s_bulk_len = (s_bulk_len * 10U) + (uint32_t)(b - '0');
		if (s_len_idx >= GS_AT_BULK_LEN_DIGITS) {
			s_bulk_got = 0U;
			s_state = (s_bulk_len == 0U) ? RX_START : RX_BULK_DATA;
			if (s_bulk_len == 0U) {
				id = GS_MSG_BULK_DATA;
			}
		}
		break;

	case RX_BULK_DATA:
		emit_data_byte(b);
		s_bulk_got++;
		if (s_bulk_got >= s_bulk_len) {
			emit_data_flush();
			id = GS_MSG_BULK_DATA;
			reset_to_start();
		}
		break;

	case RX_HTTP_CID:
		s_cid = gs_at_ascii_to_cid(b);
		s_state = RX_HTTP_LEN;
		s_len_idx = 0U;
		s_bulk_len = 0U;
		break;

	case RX_HTTP_LEN:
		if (b < '0' || b > '9') {
			reset_to_start();
			break;
		}
		s_bulk_len = (s_bulk_len * 10U) + (uint32_t)(b - '0');
		s_len_idx++;
		if (s_len_idx >= GS_AT_BULK_LEN_DIGITS) {
			s_bulk_got = 0U;
			s_state = (s_bulk_len == 0U) ? RX_START : RX_HTTP_DATA;
			if (s_bulk_len == 0U) {
				id = GS_MSG_HTTP_DATA;
			}
		}
		break;

	case RX_HTTP_DATA:
		emit_data_byte(b);
		s_bulk_got++;
		if (s_bulk_got >= s_bulk_len) {
			emit_data_flush();
			id = GS_MSG_HTTP_DATA;
			reset_to_start();
		}
		break;

	case RX_UDP_BULK_CID:
		s_cid = gs_at_ascii_to_cid(b);
		s_state = RX_UDP_BULK_IP;
		break;

	case RX_UDP_BULK_IP:
		/* Skip IP until space. */
		if (b == ' ') {
			s_state = RX_UDP_BULK_PORT;
		}
		break;

	case RX_UDP_BULK_PORT:
		/* Skip port until tab. */
		if (b == '\t') {
			s_state = RX_UDP_BULK_LEN;
			s_len_idx = 0U;
			s_bulk_len = 0U;
		}
		break;

	case RX_UDP_BULK_LEN:
		if (b < '0' || b > '9') {
			reset_to_start();
			break;
		}
		s_bulk_len = (s_bulk_len * 10U) + (uint32_t)(b - '0');
		s_len_idx++;
		if (s_len_idx >= GS_AT_BULK_LEN_DIGITS) {
			s_bulk_got = 0U;
			s_state = (s_bulk_len == 0U) ? RX_START : RX_UDP_BULK_DATA;
			if (s_bulk_len == 0U) {
				id = GS_MSG_BULK_DATA;
			}
		}
		break;

	case RX_UDP_BULK_DATA:
		emit_data_byte(b);
		s_bulk_got++;
		if (s_bulk_got >= s_bulk_len) {
			emit_data_flush();
			id = GS_MSG_BULK_DATA;
			reset_to_start();
		}
		break;

	case RX_UDP_CID:
		s_cid = gs_at_ascii_to_cid(b);
		s_state = RX_UDP_IP;
		break;

	case RX_UDP_IP:
		if (b == ' ') {
			s_state = RX_UDP_PORT;
		}
		break;

	case RX_UDP_PORT:
		if (b == '\t') {
			s_state = RX_UDP_DATA;
			s_stream_esc_pending = false;
		}
		break;

	case RX_UDP_DATA:
		if (s_stream_esc_pending) {
			s_stream_esc_pending = false;
			if (b == 'E') {
				emit_data_flush();
				id = GS_MSG_BULK_DATA;
				reset_to_start();
			} else {
				emit_data_byte(GS_AT_ESC);
				emit_data_byte(b);
			}
			break;
		}
		if (b == GS_AT_ESC) {
			s_stream_esc_pending = true;
			break;
		}
		emit_data_byte(b);
		break;

	default:
		reset_to_start();
		break;
	}

	return id;
}

gs_msg_id_t gs_at_process_chunk(const uint8_t *data, size_t len)
{
	gs_msg_id_t last = GS_MSG_NONE;
	size_t i;

	if (!data) {
		return GS_MSG_NONE;
	}
	for (i = 0; i < len; i++) {
		gs_msg_id_t id = gs_at_process_byte(data[i]);
		if (id != GS_MSG_NONE) {
			last = id;
		}
	}
	return last;
}

char gs_at_cid_to_ascii(uint8_t cid)
{
	cid &= 0x0FU;
	return (char)((cid < 10U) ? ('0' + cid) : ('A' + (cid - 10U)));
}

uint8_t gs_at_ascii_to_cid(uint8_t ascii)
{
	if (ascii >= '0' && ascii <= '9') {
		return (uint8_t)(ascii - '0');
	}
	if (ascii >= 'A' && ascii <= 'F') {
		return (uint8_t)(10 + ascii - 'A');
	}
	if (ascii >= 'a' && ascii <= 'f') {
		return (uint8_t)(10 + ascii - 'a');
	}
	return GS_AT_INVALID_CID;
}

void gs_at_u32_to_4digit(uint32_t n, char out[5])
{
	if (n > 9999U) {
		n = 9999U;
	}
	out[0] = (char)('0' + ((n / 1000U) % 10U));
	out[1] = (char)('0' + ((n / 100U) % 10U));
	out[2] = (char)('0' + ((n / 10U) % 10U));
	out[3] = (char)('0' + (n % 10U));
	out[4] = '\0';
}

const char *gs_at_last_line(void)
{
	return s_last_line;
}

const char *gs_at_last_info_line(void)
{
	return s_last_info_line;
}

const char *gs_at_info_accum(void)
{
	return s_info_accum;
}

void gs_at_clear_info_accum(void)
{
	clear_info_accum();
}

const char *gs_at_partial_line(void)
{
	s_line[s_line_len < sizeof(s_line) ? s_line_len : (sizeof(s_line) - 1U)] =
		'\0';
	return s_line;
}

uint32_t gs_at_rx_byte_count(void)
{
	return s_rx_bytes;
}

uint8_t gs_at_parse_connect_cid(void)
{
	const char *p = strstr(s_last_line, "CONNECT");
	if (!p) {
		return GS_AT_INVALID_CID;
	}
	p += 7;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	if (*p == '\0') {
		return GS_AT_INVALID_CID;
	}
	return gs_at_ascii_to_cid((uint8_t)*p);
}

void gs_at_flush(void)
{
	const gs_platform_t *p = gs_platform_get();
	if (p && p->uart_flush) {
		p->uart_flush(p->ctx);
	}
	reset_to_start();
	s_line_len = 0U;
	s_last_line[0] = '\0';
	s_last_info_line[0] = '\0';
	clear_info_accum();
	s_rx_bytes = 0U;
}
