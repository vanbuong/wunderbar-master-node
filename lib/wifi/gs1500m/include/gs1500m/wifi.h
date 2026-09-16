/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#ifndef GS1500M_WIFI_H
#define GS1500M_WIFI_H

#include "gs1500m/at.h"

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
 * Bring-up sequence:
 * INTF_SEL UART → PGM idle → HW reset pulse → wait ready (≥2s) →
 * flush → AT probe (optional AT+RESET) → ATE0 → AT+BDATA=1 → radio on →
 * query module info (VER / MAC).
 */
gs_msg_id_t gs_wifi_init(uint32_t ready_timeout_ms);

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
