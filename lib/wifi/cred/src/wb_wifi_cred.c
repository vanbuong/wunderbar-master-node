/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_wifi_cred.h"

#include <string.h>

/*
 * Placed at WB_WIFI_CRED_FLASH_ADDR via linker (Zephyr WIFI_CRED region or
 * FreeRTOS --section-start). Accessors use the linked symbol so the runtime
 * view always matches what patch_wifi_cred.py patches in the .elf section.
 */
__attribute__((section(".wb_wifi_cred"), used, aligned(4)))
const wb_wifi_cred_t wb_wifi_cred = {
	.magic = { 'W', 'B', 'W', 'I', 'F', 'I', 'v', '1' },
	.ssid = { 0 },
	.psk = { 0 },
	.reserved = { 0 },
};

const wb_wifi_cred_t *wb_wifi_cred_at_flash(void)
{
	return &wb_wifi_cred;
}

static bool magic_ok(const wb_wifi_cred_t *c)
{
	const char *m;

	if (!c) {
		return false;
	}
	m = c->magic;
	return m[0] == 'W' && m[1] == 'B' && m[2] == 'W' && m[3] == 'I' &&
	       m[4] == 'F' && m[5] == 'I' && m[6] == 'v' && m[7] == '1';
}

bool wb_wifi_cred_valid(void)
{
	const wb_wifi_cred_t *c = wb_wifi_cred_at_flash();
	return magic_ok(c) && c->ssid[0] != '\0';
}

const char *wb_wifi_cred_ssid(void)
{
	const wb_wifi_cred_t *c = wb_wifi_cred_at_flash();
	return (magic_ok(c) && c->ssid[0] != '\0') ? c->ssid : NULL;
}

const char *wb_wifi_cred_psk(void)
{
	const wb_wifi_cred_t *c = wb_wifi_cred_at_flash();
	if (!magic_ok(c)) {
		return NULL;
	}
	return c->psk;
}
