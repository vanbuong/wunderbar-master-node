/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Fixed flash slot for WiFi STA credentials (patchable in .bin/.elf).
 *
 * Layout (128 bytes) at WB_WIFI_CRED_FLASH_ADDR (default 0x0007E000):
 *   [0..7]   magic "WBWIFIv1"
 *   [8..40]  ssid  (32 chars + NUL)
 *   [41..105] psk  (64 chars + NUL)
 *   [106..127] reserved / padding
 *
 * Patch with: ./scripts/patch_wifi_cred.py <image> --ssid ... --psk ...
 */

#ifndef WB_WIFI_CRED_H
#define WB_WIFI_CRED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WB_WIFI_CRED_FLASH_ADDR
#define WB_WIFI_CRED_FLASH_ADDR 0x0007E000u
#endif

#define WB_WIFI_CRED_MAGIC_0 'W'
#define WB_WIFI_CRED_MAGIC_1 'B'
#define WB_WIFI_CRED_MAGIC_2 'W'
#define WB_WIFI_CRED_MAGIC_3 'I'
#define WB_WIFI_CRED_MAGIC_4 'F'
#define WB_WIFI_CRED_MAGIC_5 'I'
#define WB_WIFI_CRED_MAGIC_6 'v'
#define WB_WIFI_CRED_MAGIC_7 '1'
/* Documentation-only string; firmware must not reference this literal. */
#define WB_WIFI_CRED_MAGIC_STR "WBWIFIv1"
#define WB_WIFI_CRED_MAGIC_LEN  8u
#define WB_WIFI_CRED_SSID_MAX   32u
#define WB_WIFI_CRED_PSK_MAX    64u
#define WB_WIFI_CRED_SIZE       128u

typedef struct __attribute__((packed)) {
	char magic[WB_WIFI_CRED_MAGIC_LEN];
	char ssid[WB_WIFI_CRED_SSID_MAX + 1u];
	char psk[WB_WIFI_CRED_PSK_MAX + 1u];
	uint8_t reserved[WB_WIFI_CRED_SIZE - WB_WIFI_CRED_MAGIC_LEN -
			 (WB_WIFI_CRED_SSID_MAX + 1u) - (WB_WIFI_CRED_PSK_MAX + 1u)];
} wb_wifi_cred_t;

_Static_assert(sizeof(wb_wifi_cred_t) == WB_WIFI_CRED_SIZE,
	       "wb_wifi_cred_t must be 128 bytes");

/** Linked credential blob (section .wb_wifi_cred @ WB_WIFI_CRED_FLASH_ADDR). */
extern const wb_wifi_cred_t wb_wifi_cred;

/** Pointer to the linked flash slot (same object the patch script updates). */
const wb_wifi_cred_t *wb_wifi_cred_at_flash(void);

/** True if magic matches and ssid is non-empty. */
bool wb_wifi_cred_valid(void);

/** SSID pointer if valid, else NULL. */
const char *wb_wifi_cred_ssid(void);

/** PSK pointer if valid (may be empty string for open), else NULL. */
const char *wb_wifi_cred_psk(void);

#ifdef __cplusplus
}
#endif

#endif /* WB_WIFI_CRED_H */
