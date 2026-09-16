/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * GS1500M Serial2WiFi AT parser + command helpers.
 */

#ifndef GS1500M_AT_H
#define GS1500M_AT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GS_AT_ESC                 0x1BU
#define GS_AT_INVALID_CID         0xFFU
#define GS_AT_TX_CMD_MAX          256U
#define GS_AT_RX_LINE_MAX         512U
#define GS_AT_BULK_LEN_DIGITS     4U
#define GS_AT_DEFAULT_CMD_TIMEOUT_MS 10000U

typedef enum {
	GS_MSG_NONE = 0,
	GS_MSG_OK,
	GS_MSG_ERROR,
	GS_MSG_INVALID_INPUT,
	GS_MSG_ERROR_IP_CONFIG,
	GS_MSG_ERROR_SOCKET,
	GS_MSG_DISCONNECT,
	GS_MSG_DISASSOCIATED,
	GS_MSG_APP_RESET,
	GS_MSG_WELCOME,
	GS_MSG_CONNECT,
	GS_MSG_CONNECT_SERVER_CLIENT,
	GS_MSG_TIMEOUT,
	GS_MSG_BULK_DATA,
	GS_MSG_STREAM_DATA,
	GS_MSG_HTTP_DATA,
	GS_MSG_ESC_OK,
	GS_MSG_ESC_FAIL,
	GS_MSG_FW_UPDATE_OK,
} gs_msg_id_t;

typedef enum {
	GS_ESC_KIND_NONE = 0,
	GS_ESC_KIND_STREAM,   /* ESC S */
	GS_ESC_KIND_BULK,     /* ESC Z */
	GS_ESC_KIND_UDP_BULK, /* ESC Y */
	GS_ESC_KIND_UDP,      /* ESC u */
	GS_ESC_KIND_HTTP,     /* ESC H */
} gs_esc_kind_t;

/** Called for each payload byte of an ESC data frame. */
typedef void (*gs_data_cb_t)(uint8_t cid, gs_esc_kind_t kind,
			     const uint8_t *data, size_t len, void *user);

/** Optional line callback for async unsolicited messages. */
typedef void (*gs_line_cb_t)(gs_msg_id_t id, const char *line, void *user);

typedef struct {
	gs_data_cb_t on_data;
	gs_line_cb_t on_line;
	void *user;
} gs_at_callbacks_t;

void gs_at_init(const gs_at_callbacks_t *cbs);
void gs_at_set_callbacks(const gs_at_callbacks_t *cbs);

/** Feed one RX byte into the line/ESC FSM. */
gs_msg_id_t gs_at_process_byte(uint8_t b);

/** Feed a chunk (convenience). Returns last non-NONE msg if any. */
gs_msg_id_t gs_at_process_chunk(const uint8_t *data, size_t len);

/** Classify a completed CR/LF-terminated line (no need for live UART). */
gs_msg_id_t gs_at_classify_line(const char *line);

/** Drain UART via platform until timeout or OK/ERROR-class response. */
gs_msg_id_t gs_at_wait_response(uint32_t timeout_ms);

/** Send raw bytes on UART. */
int gs_at_write(const uint8_t *data, size_t len);

/** Send a NUL-terminated AT command (include \\r\\n yourself or use helpers). */
gs_msg_id_t gs_at_send_cmd(const char *cmd, uint32_t timeout_ms);

/** Format+send "AT...\\r\\n" with printf-style args. */
gs_msg_id_t gs_at_send_cmdf(uint32_t timeout_ms, const char *fmt, ...);

/** Last completed response line (valid until next command). */
const char *gs_at_last_line(void);

/**
 * Last informational line before a terminal OK/ERROR (e.g. RSSI value,
 * NSTAT fields). Cleared on gs_at_init / flush.
 */
const char *gs_at_last_info_line(void);

/**
 * All informational lines from the last command (joined with '\\n'), up to
 * the terminating OK/ERROR. Cleared at the start of each send/wait and on
 * flush/init. Useful for multi-line AT+VER=? responses.
 */
const char *gs_at_info_accum(void);

/** Clear the multi-line info accumulator (also done by send/wait/flush). */
void gs_at_clear_info_accum(void);

/**
 * Bytes currently buffered in an incomplete line (no CR/LF yet).
 * Useful when diagnosing TIMEOUT with empty last_line.
 */
const char *gs_at_partial_line(void);

/** Total UART bytes fed to the parser since gs_at_init / flush. */
uint32_t gs_at_rx_byte_count(void);

/** Parse CONNECT cid from last response; returns GS_AT_INVALID_CID on failure. */
uint8_t gs_at_parse_connect_cid(void);

/** Convert 0–15 cid to ASCII hex digit. */
char gs_at_cid_to_ascii(uint8_t cid);

/** Parse ASCII hex digit to cid. */
uint8_t gs_at_ascii_to_cid(uint8_t ascii);

/** Write 4-digit decimal ASCII length (bulk framing). */
void gs_at_u32_to_4digit(uint32_t n, char out[5]);

/** Flush UART RX and reset parser state. */
void gs_at_flush(void);

/** ESC Z bulk TX (waits for ESC OK/FAIL or line OK). */
gs_msg_id_t gs_at_send_bulk(uint8_t cid, const uint8_t *data, size_t len,
			    uint32_t timeout_ms);

/** ESC S ... ESC E stream TX. */
gs_msg_id_t gs_at_send_stream(uint8_t cid, const uint8_t *data, size_t len,
			      uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_AT_H */
