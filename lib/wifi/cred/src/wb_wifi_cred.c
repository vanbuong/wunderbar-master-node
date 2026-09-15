/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_wifi_cred.h"

#include <string.h>

/*
 * Placed at WB_WIFI_CRED_FLASH_ADDR via linker --section-start / snippet.
 * Magic bytes exist ONLY in this object (no second "WBWIFIv1" string in
 * .rodata) so patch_wifi_cred.py can locate the slot uniquely.
 */
__attribute__((section(".wb_wifi_cred"), used, aligned(4)))
const wb_wifi_cred_t wb_wifi_cred = {
	.magic = { 'W', 'B', 'W', 'I', 'F', 'I', 'v', '1' },
	.ssid = { 0 },
	.psk = { 0 },
	.reserved = { 0 },
};

static bool magic_ok(void)
{
	const char *m = wb_wifi_cred.magic;
	return m[0] == 'W' && m[1] == 'B' && m[2] == 'W' && m[3] == 'I' &&
	       m[4] == 'F' && m[5] == 'I' && m[6] == 'v' && m[7] == '1';
}

bool wb_wifi_cred_valid(void)
{
	return magic_ok() && wb_wifi_cred.ssid[0] != '\0';
}

const char *wb_wifi_cred_ssid(void)
{
	return wb_wifi_cred_valid() ? wb_wifi_cred.ssid : NULL;
}

const char *wb_wifi_cred_psk(void)
{
	if (!magic_ok()) {
		return NULL;
	}
	return wb_wifi_cred.psk;
}
