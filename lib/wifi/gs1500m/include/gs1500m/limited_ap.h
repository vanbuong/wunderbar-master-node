/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Limited AP + web provisioning helpers.
 */

#ifndef GS1500M_LIMITED_AP_H
#define GS1500M_LIMITED_AP_H

#include "gs1500m/at.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Start Limited AP:
 * NSET → WM=2 → WA=<ssid> → DHCPSRVR=1 → optional WEBPROV.
 */
gs_msg_id_t gs_lap_start(const char *ssid, const char *channel,
			 const char *ip, const char *mask, const char *gw);

gs_msg_id_t gs_lap_dhcp_server(bool on);
gs_msg_id_t gs_lap_webprov(const char *username, const char *password);
gs_msg_id_t gs_lap_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* GS1500M_LIMITED_AP_H */
