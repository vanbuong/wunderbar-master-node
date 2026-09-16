/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 *
 * ztest suite for GS1500M WiFi library (stub UART platform).
 */

#include <zephyr/ztest.h>
#include "gs_stub_platform.h"

#include "gs1500m/at.h"
#include "gs1500m/wifi.h"
#include "gs1500m/socket.h"
#include "gs1500m/ssl.h"
#include "gs1500m/mqtt_pipe.h"
#include "gs1500m/limited_ap.h"
#include "gs1500m/user.h"

#include <string.h>

static gs_stub_ctx_t s_stub;
static gs_platform_t s_plat;

static void *wifi_setup(void)
{
	gs_stub_reset(&s_stub);
	gs_stub_install(&s_stub, &s_plat);
	gs_at_init(NULL);
	return NULL;
}

static void wifi_before(void *fixture)
{
	(void)fixture;
	gs_stub_reset(&s_stub);
	gs_stub_install(&s_stub, &s_plat);
	gs_at_init(NULL);
}

ZTEST_SUITE(gs1500m_wifi_tests, NULL, wifi_setup, wifi_before, NULL, NULL);

ZTEST(gs1500m_wifi_tests, test_init_bringup)
{
	zassert_equal(gs_wifi_init(1000U), GS_MSG_OK);
	zassert_true(s_stub.last_intf_uart);
	/* PE-style bring-up may succeed without a HW reset pulse. */
	zassert_true(s_stub.reset_pulses <= 1U);
	zassert_true(gs_stub_tx_contains(&s_stub, "ATE0\r\n"));
	zassert_true(gs_stub_tx_contains(&s_stub, "AT+WRXACTIVE=1\r\n"));
	zassert_false(gs_stub_tx_contains(&s_stub, "AT+BDATA=1\r\n"));
}

ZTEST(gs1500m_wifi_tests, test_join_wpa)
{
	zassert_equal(gs_wifi_join_wpa("SSID", "passw0rd"), GS_MSG_OK);
	zassert_true(gs_stub_tx_contains(&s_stub, "AT+WPAPSK=SSID,passw0rd\r\n") ||
		     gs_stub_tx_contains(&s_stub, "AT+WWPA=passw0rd\r\n"));
	zassert_true(gs_stub_tx_contains(&s_stub, "AT+WA=SSID\r\n"));
}

ZTEST(gs1500m_wifi_tests, test_tcp_and_bulk)
{
	uint8_t cid = GS_AT_INVALID_CID;
	const uint8_t hi[] = { 'o', 'k' };
	gs_msg_id_t id = gs_socket_tcp_client("8.8.8.8", 443, &cid);

	zassert_true(id == GS_MSG_CONNECT || id == GS_MSG_OK);
	zassert_equal(cid, 1);
	zassert_equal(gs_socket_send(cid, hi, sizeof(hi)), GS_MSG_OK);
	zassert_true(gs_stub_tx_contains(&s_stub, "\x1bZ1"));
}

ZTEST(gs1500m_wifi_tests, test_mqtt_pipe)
{
	gs_mqtt_pipe_t pipe;

	zassert_equal(gs_mqtt_pipe_open(&pipe, "host", 8883, true, "ca"), GS_MSG_OK);
	zassert_true(pipe.open && pipe.tls);
	zassert_equal(gs_mqtt_pipe_close(&pipe), GS_MSG_OK);
}

ZTEST(gs1500m_wifi_tests, test_limited_ap)
{
	zassert_equal(gs_lap_start("AP", "1", NULL, NULL, NULL), GS_MSG_OK);
	zassert_true(gs_stub_tx_contains(&s_stub, "AT+WM=2\r\n"));
	zassert_true(gs_stub_tx_contains(&s_stub, "AT+DHCPSRVR=1\r\n"));
}

ZTEST(gs1500m_wifi_tests, test_user_ready)
{
	gs_user_t user;
	gs_user_config_t cfg;
	gs_user_state_t st = GS_USER_IDLE;
	int i;

	memset(&cfg, 0, sizeof(cfg));
	cfg.ssid = "A";
	cfg.psk = "B";
	gs_user_init(&user, &cfg);

	for (i = 0; i < 20; i++) {
		st = gs_user_poll(&user);
		if (st == GS_USER_READY || st == GS_USER_ERROR) {
			break;
		}
	}
	zassert_equal(st, GS_USER_READY);
}

ZTEST(gs1500m_wifi_tests, test_ssl_open)
{
	zassert_equal(gs_ssl_open(2, "root", NULL, NULL), GS_MSG_OK);
	zassert_true(gs_stub_tx_contains(&s_stub, "AT+SSLOPEN=2,root\r\n"));
}
