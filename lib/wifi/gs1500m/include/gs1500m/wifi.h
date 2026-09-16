/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef GS1500M_WIFI_H
#define GS1500M_WIFI_H

#include "gs1500m/at.h"
#include "gs1500m/platform.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	GS_WIFI_SEC_OPEN = 0,
	GS_WIFI_SEC_WEP = 1,
	GS_WIFI_SEC_WPA_PSK = 2,
	GS_WIFI_SEC_WPA2_PSK = 4,
	GS_WIFI_SEC_WPA_WPA2_PSK = 8,
} gs_wifi_security_t;

typedef enum {
	GS_WIFI_MODE_STA = 0,
	GS_WIFI_MODE_ADHOC = 1,
	GS_WIFI_MODE_LIMITED_AP = 2,
} gs_wifi_mode_t;

typedef struct {
	char ip[16];
	char subnet[16];
	char gateway[16];
	char dns[16];
	char mac[18];
	char ssid[33];
	bool associated;
} gs_wifi_status_t;

/**
 * Bring-up sequence (multi-strategy):
 * Try INTF_SEL float/1/0 × baud 115200/9600 with HW reset + AT probe, then
 * ATE0 → AT+BDATA=1 → radio on → query module info (VER / MAC).
 * If the link opens at 9600, host switches to 115200 via ATB=115200.
 */
gs_msg_id_t gs_wifi_init(uint32_t ready_timeout_ms);

/** Diagnostics from the last gs_wifi_init attempt. */
typedef struct {
	uint32_t baud;       /**< Host UART baud that answered AT (0 if none) */
	int intf_sel;        /**< -1=float, 0, or 1 */
	int8_t pgm_idle;     /**< -1=float (PE), 0=drive low, 1=drive high */
	bool hw_reset;       /**< Pulsed PTD5 during this attempt */
	bool saw_boot;       /**< Saw Serial2WiFi / APP Reset banner */
	uint32_t rx_bytes;   /**< Parser RX count after last probe */
	gs_ctrl_pins_t pins; /**< Control GPIO sample after last attempt */
} gs_wifi_init_diag_t;

const gs_wifi_init_diag_t *gs_wifi_last_init_diag(void);

gs_msg_id_t gs_wifi_echo(bool on);
gs_msg_id_t gs_wifi_soft_reset(void);
gs_msg_id_t gs_wifi_version(char *out, size_t out_len);

/** Identity reported by the GS1500M Serial2WiFi firmware. */
typedef struct {
	char name[32];     /**< e.g. "Serial2WiFi" / "GS1500M" */
	char version[160]; /**< Full AT+VER=? text (multi-line, '\\n'-joined) */
	char app_ver[32];  /**< S2W APP VERSION=... */
	char geps_ver[32]; /**< S2W GEPS VERSION=... */
	char wlan_ver[32]; /**< S2W WLAN VERSION=... */
	char mac[18];      /**< AT+NMAC=? (aa:bb:cc:dd:ee:ff) */
} gs_wifi_module_info_t;

/**
 * Query AT+VER=? and AT+NMAC=?; fill @p out and cache for
 * gs_wifi_last_module_info(). Best-effort: partial fills still return OK
 * if at least one query succeeded.
 */
gs_msg_id_t gs_wifi_query_module_info(gs_wifi_module_info_t *out);

/** Last module info from init / query_module_info (may be empty). */
const gs_wifi_module_info_t *gs_wifi_last_module_info(void);

gs_msg_id_t gs_wifi_radio(bool on);
gs_msg_id_t gs_wifi_bulk_data(bool on);
gs_msg_id_t gs_wifi_set_mode(gs_wifi_mode_t mode);
gs_msg_id_t gs_wifi_set_security(gs_wifi_security_t sec);
gs_msg_id_t gs_wifi_set_passphrase(const char *psk);
gs_msg_id_t gs_wifi_dhcp_client(bool on);
gs_msg_id_t gs_wifi_set_ip(const char *ip, const char *mask, const char *gw);

gs_msg_id_t gs_wifi_join(const char *ssid, const char *bssid_or_null,
			 const char *channel_or_null);
gs_msg_id_t gs_wifi_disconnect(void);
gs_msg_id_t gs_wifi_get_status(gs_wifi_status_t *st);
gs_msg_id_t gs_wifi_get_ip(char *ip, size_t ip_len);
gs_msg_id_t gs_wifi_get_rssi(int16_t *rssi_dbm);

/** Convenience: security + PSK + DHCP + join for infrastructure STA. */
gs_msg_id_t gs_wifi_join_wpa(const char *ssid, const char *psk);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_WIFI_H */
