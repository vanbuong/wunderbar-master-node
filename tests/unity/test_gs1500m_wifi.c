/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Unity host tests for the portable GS1500M WiFi library (stub UART).
 */

#include "unity.h"
#include "gs_stub_platform.h"

#include "gs1500m/at.h"
#include "gs1500m/wifi.h"
#include "gs1500m/socket.h"
#include "gs1500m/ssl.h"
#include "gs1500m/http.h"
#include "gs1500m/mqtt_pipe.h"
#include "gs1500m/limited_ap.h"
#include "gs1500m/user.h"
#include "wb_time.h"

#include <string.h>

static gs_stub_ctx_t s_stub;
static gs_platform_t s_plat;

void setUp(void)
{
	gs_stub_reset(&s_stub);
	gs_stub_install(&s_stub, &s_plat);
	gs_at_init(NULL);
	wb_time_init(NULL, NULL);
	wb_time_set_unix(0);
}

void tearDown(void)
{
}

void test_wifi_init_pulses_reset_and_sends_bringup_cmds(void)
{
	gs_msg_id_t id = gs_wifi_init(1000U);

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, id);
	TEST_ASSERT_TRUE(s_stub.last_intf_uart);
	TEST_ASSERT_FALSE(s_stub.last_pgm_assert);
	/* PE leaves RESET as input; first successful try may skip HW pulse. */
	TEST_ASSERT_TRUE(s_stub.reset_pulses <= 1U);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "ATE0\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WRXACTIVE=1\r\n"));
	/* Bulk mode is deferred until after join. */
	TEST_ASSERT_FALSE(gs_stub_tx_contains(&s_stub, "AT+BDATA=1\r\n"));
	/* VER/NMAC are queried after join, not during init. */
	TEST_ASSERT_FALSE(gs_stub_tx_contains(&s_stub, "AT+VER="));
}

void test_wifi_query_module_info_parses_ver_mac(void)
{
	gs_wifi_module_info_t mi;

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_wifi_query_module_info(&mi));
	TEST_ASSERT_EQUAL_STRING("Serial2WiFi", mi.name);
	TEST_ASSERT_EQUAL_STRING("2.5.1", mi.app_ver);
	TEST_ASSERT_EQUAL_STRING("2.5.1", mi.geps_ver);
	TEST_ASSERT_EQUAL_STRING("2.5.0", mi.wlan_ver);
	TEST_ASSERT_EQUAL_STRING("00:1d:c9:12:34:56", mi.mac);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+VER="));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NMAC=?\r\n"));
}

void test_wifi_join_wpa_command_sequence(void)
{
	gs_msg_id_t id = gs_wifi_join_wpa("CafeWifi", "secret12");

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, id);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WM=0\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WPAPSK=CafeWifi,secret12\r\n") ||
			(gs_stub_tx_contains(&s_stub, "AT+WSEC=8\r\n") &&
			 gs_stub_tx_contains(&s_stub, "AT+WWPA=secret12\r\n")));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NDHCP=1\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WA=CafeWifi\r\n"));
}

void test_wifi_join_with_channel(void)
{
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_wifi_join("AP", NULL, "6"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WA=AP,,6\r\n"));
}

void test_wifi_disconnect_and_echo(void)
{
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_wifi_disconnect());
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WD\r\n"));
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_wifi_echo(true));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "ATE1\r\n"));
}

void test_wifi_get_rssi(void)
{
	int16_t rssi = 0;

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_wifi_get_rssi(&rssi));
	TEST_ASSERT_EQUAL_INT16(-45, rssi);
}

void test_wifi_invalid_args(void)
{
	TEST_ASSERT_EQUAL_INT(GS_MSG_INVALID_INPUT, gs_wifi_join(NULL, NULL, NULL));
	TEST_ASSERT_EQUAL_INT(GS_MSG_INVALID_INPUT, gs_wifi_set_passphrase(NULL));
	TEST_ASSERT_EQUAL_INT(GS_MSG_INVALID_INPUT,
			      gs_wifi_set_ip(NULL, "255.255.255.0", "1.1.1.1"));
}

void test_socket_tcp_client_parses_cid(void)
{
	uint8_t cid = GS_AT_INVALID_CID;
	gs_msg_id_t id = gs_socket_tcp_client("10.0.0.2", 1883, &cid);

	TEST_ASSERT_TRUE(id == GS_MSG_CONNECT || id == GS_MSG_OK);
	TEST_ASSERT_EQUAL_UINT8(1, cid);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NCTCP=10.0.0.2,1883\r\n"));
}

void test_socket_udp_server_close(void)
{
	uint8_t cid = GS_AT_INVALID_CID;
	gs_msg_id_t id;

	id = gs_socket_udp_client("1.2.3.4", 53, 0, &cid);
	TEST_ASSERT_TRUE(id == GS_MSG_CONNECT || id == GS_MSG_OK);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NCUDP=1.2.3.4,53,0\r\n"));

	s_stub.tx_len = 0;
	id = gs_socket_tcp_server(80, &cid);
	TEST_ASSERT_TRUE(id == GS_MSG_CONNECT || id == GS_MSG_OK);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NSTCP=80\r\n"));

	s_stub.tx_len = 0;
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_socket_close(1));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NCLOSE=1\r\n"));
}

void test_socket_send_bulk_esc_z(void)
{
	const uint8_t payload[] = { 'h', 'i' };
	gs_msg_id_t id = gs_socket_send(1, payload, sizeof(payload));

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, id);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "\x1bZ1"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "hi"));
}

void test_ssl_open_and_cert_delete(void)
{
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_ssl_open(1, "ca", NULL, NULL));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+SSLOPEN=1,ca\r\n"));

	s_stub.tx_len = 0;
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_ssl_cert_delete("ca"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+TCERTDEL=ca\r\n"));
}

void test_ssl_cert_add_esc_w(void)
{
	const uint8_t der[] = { 0x30, 0x03, 0x01 };
	gs_msg_id_t id = gs_ssl_cert_add("ca", true, der, sizeof(der));

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, id);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+TCERTADD=ca,0,3,0\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "\x1bW"));
}

void test_http_open_send_get(void)
{
	uint8_t cid = GS_AT_INVALID_CID;
	gs_msg_id_t id = gs_http_open("example.com", 80, false, NULL, &cid);

	TEST_ASSERT_TRUE(id == GS_MSG_CONNECT || id == GS_MSG_OK);
	TEST_ASSERT_EQUAL_UINT8(1, cid);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+HTTPOPEN=example.com,80\r\n"));

	s_stub.tx_len = 0;
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_http_send(1, 1, 5, "/time", NULL, 0));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+HTTPSEND=1,1,5,/time\r\n"));
}

void test_mqtt_pipe_open_tls_send_close(void)
{
	gs_mqtt_pipe_t pipe;
	const uint8_t pkt[] = { 0x10, 0x02 };

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK,
			      gs_mqtt_pipe_open(&pipe, "mqtt.example", 8883, true, "ca"));
	TEST_ASSERT_TRUE(pipe.open);
	TEST_ASSERT_TRUE(pipe.tls);
	TEST_ASSERT_EQUAL_UINT8(1, pipe.cid);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NCTCP=mqtt.example,8883\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+SSLOPEN=1,ca\r\n"));

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_mqtt_pipe_send(&pipe, pkt, sizeof(pkt)));
	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_mqtt_pipe_close(&pipe));
	TEST_ASSERT_FALSE(pipe.open);
}

void test_limited_ap_start_sequence(void)
{
	gs_msg_id_t id = gs_lap_start("WunderBar-Setup", "6", "192.168.240.1",
				      "255.255.255.0", NULL);

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, id);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NDHCP=0\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(
		&s_stub, "AT+NSET=192.168.240.1,255.255.255.0,192.168.240.1\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WM=2\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WA=WunderBar-Setup,,6\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+DHCPSRVR=1\r\n"));
}

void test_wifi_get_status_parses_ip_addr(void)
{
	gs_wifi_status_t st;

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK, gs_wifi_get_status(&st));
	TEST_ASSERT_TRUE(st.associated);
	TEST_ASSERT_EQUAL_STRING("192.168.1.50", st.ip);
	TEST_ASSERT_EQUAL_STRING("192.168.1.50", gs_wifi_last_ip());
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NSTAT=?\r\n"));
}

void test_wifi_ntp_sync_sets_time(void)
{
	uint32_t unix_sec = 0;
	char time_str[40];

	TEST_ASSERT_EQUAL_INT(GS_MSG_OK,
			      gs_wifi_ntp_sync(&unix_sec, time_str, sizeof(time_str)));
	TEST_ASSERT_EQUAL_UINT(1758038400U, unix_sec);
	TEST_ASSERT_TRUE(time_str[0] != '\0');
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NCUDP="));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+SETTIME="));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+GETTIME=?\r\n"));
	TEST_ASSERT_EQUAL_STRING(time_str, gs_wifi_last_time_str());
}

void test_user_sm_queries_ip_and_ntp(void)
{
	gs_user_t user;
	gs_user_config_t cfg;
	gs_user_state_t st = GS_USER_IDLE;
	int steps;

	memset(&cfg, 0, sizeof(cfg));
	cfg.ssid = "TestSSID";
	cfg.psk = "password";
	gs_user_init(&user, &cfg);

	for (steps = 0; steps < 20; steps++) {
		st = gs_user_poll(&user);
		if (st == GS_USER_READY || st == GS_USER_ERROR) {
			break;
		}
	}

	TEST_ASSERT_EQUAL_INT(GS_USER_READY, st);
	TEST_ASSERT_EQUAL_STRING("192.168.1.50", gs_wifi_last_ip());
	TEST_ASSERT_TRUE(gs_wifi_last_unix_time() != 0U);
	TEST_ASSERT_TRUE(wb_time_is_synced());
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+BDATA=1\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+VER="));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+NMAC=?\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+GETTIME=?\r\n"));
	{
		const gs_wifi_module_info_t *mi = gs_wifi_last_module_info();
		TEST_ASSERT_NOT_NULL(mi);
		TEST_ASSERT_EQUAL_STRING("Serial2WiFi", mi->name);
		TEST_ASSERT_EQUAL_STRING("2.5.1", mi->app_ver);
	}
}

void test_user_sm_lap_fallback_on_join_error(void)
{
	gs_user_t user;
	gs_user_config_t cfg;
	gs_user_state_t st;

	memset(&cfg, 0, sizeof(cfg));
	cfg.ssid = "NoJoin";
	cfg.psk = "x";
	cfg.use_limited_ap_on_fail = true;
	cfg.lap_ssid = "WB-AP";
	cfg.lap_ip = "192.168.240.1";
	gs_user_init(&user, &cfg);

	st = gs_user_poll(&user); /* IDLE → INIT */
	TEST_ASSERT_EQUAL_INT(GS_USER_INIT, st);
	st = gs_user_poll(&user); /* run INIT → JOIN */
	TEST_ASSERT_EQUAL_INT(GS_USER_JOIN, st);

	/* Fail join: first AT of join_wpa gets ERROR */
	s_stub.auto_ok = false;
	gs_stub_rx_push_str(&s_stub, "ERROR\r\n");
	st = gs_user_poll(&user);
	TEST_ASSERT_EQUAL_INT(GS_USER_LIMITED_AP, st);

	s_stub.auto_ok = true;
	st = gs_user_poll(&user); /* run LIMITED_AP → READY */
	TEST_ASSERT_EQUAL_INT(GS_USER_READY, st);
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WM=2\r\n"));
	TEST_ASSERT_TRUE(gs_stub_tx_contains(&s_stub, "AT+WA=WB-AP,,6\r\n"));
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_wifi_init_pulses_reset_and_sends_bringup_cmds);
	RUN_TEST(test_wifi_query_module_info_parses_ver_mac);
	RUN_TEST(test_wifi_join_wpa_command_sequence);
	RUN_TEST(test_wifi_join_with_channel);
	RUN_TEST(test_wifi_disconnect_and_echo);
	RUN_TEST(test_wifi_get_rssi);
	RUN_TEST(test_wifi_invalid_args);
	RUN_TEST(test_socket_tcp_client_parses_cid);
	RUN_TEST(test_socket_udp_server_close);
	RUN_TEST(test_socket_send_bulk_esc_z);
	RUN_TEST(test_ssl_open_and_cert_delete);
	RUN_TEST(test_ssl_cert_add_esc_w);
	RUN_TEST(test_http_open_send_get);
	RUN_TEST(test_mqtt_pipe_open_tls_send_close);
	RUN_TEST(test_limited_ap_start_sequence);
	RUN_TEST(test_wifi_get_status_parses_ip_addr);
	RUN_TEST(test_wifi_ntp_sync_sets_time);
	RUN_TEST(test_user_sm_queries_ip_and_ntp);
	RUN_TEST(test_user_sm_lap_fallback_on_join_error);
	return UNITY_END();
}
