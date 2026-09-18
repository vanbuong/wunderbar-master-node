/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 */

#include "wb_time.h"

#include <stdio.h>
#include <stdint.h>

static wb_time_millis_fn s_millis;
static void *s_millis_ctx;
static bool s_synced;
static uint32_t s_unix_at_sync;
static uint32_t s_millis_at_sync;

static uint32_t now_ms(void)
{
	return s_millis ? s_millis(s_millis_ctx) : 0U;
}

static void unix_to_ymdhms(uint32_t unix_sec, int *y, int *mo, int *d,
			   int *hh, int *mm, int *ss)
{
	/* Civil from days (Howard Hinnant) — UTC, no leap seconds. */
	int64_t z;
	int64_t era;
	unsigned doe;
	unsigned yoe;
	unsigned doy;
	unsigned mp;
	int year;
	int month;
	int day;

	*ss = (int)(unix_sec % 60U);
	unix_sec /= 60U;
	*mm = (int)(unix_sec % 60U);
	unix_sec /= 60U;
	*hh = (int)(unix_sec % 24U);
	unix_sec /= 24U;

	z = (int64_t)unix_sec + 719468LL;
	era = (z >= 0) ? (z / 146097LL) : ((z - 146096LL) / 146097LL);
	doe = (unsigned)(z - era * 146097LL);
	yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
	year = (int)(yoe) + (int)(era * 400LL);
	doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
	mp = (5U * doy + 2U) / 153U;
	day = (int)(doy - (153U * mp + 2U) / 5U) + 1;
	month = (int)(mp < 10U ? (mp + 3U) : (mp - 9U));
	year += (month <= 2) ? 1 : 0;

	*y = year;
	*mo = month;
	*d = day;
}

void wb_time_init(wb_time_millis_fn millis, void *ctx)
{
	s_millis = millis;
	s_millis_ctx = ctx;
}

bool wb_time_is_synced(void)
{
	return s_synced;
}

void wb_time_set_unix(uint32_t unix_sec)
{
	s_unix_at_sync = unix_sec;
	s_millis_at_sync = now_ms();
	s_synced = (unix_sec != 0U);
}

uint64_t wb_time_get_unix_ms(void)
{
	uint32_t ms = now_ms();

	if (!s_synced) {
		return (uint64_t)ms;
	}
	/* Unsigned delta handles tick wrap. */
	return ((uint64_t)s_unix_at_sync * 1000ULL) +
	       (uint64_t)(ms - s_millis_at_sync);
}

uint32_t wb_time_get_unix(void)
{
	if (!s_synced) {
		return 0U;
	}
	return (uint32_t)(wb_time_get_unix_ms() / 1000ULL);
}

int wb_time_format_utc(uint32_t unix_sec, char *buf, size_t buflen)
{
	int y, mo, d, hh, mm, ss;

	if (!buf || buflen < 20U) {
		return 0;
	}
	unix_to_ymdhms(unix_sec, &y, &mo, &d, &hh, &mm, &ss);
	return snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d", y, mo, d,
			hh, mm, ss);
}

int wb_time_format_log(char *buf, size_t buflen)
{
	uint64_t t_ms;
	uint32_t sec;
	uint32_t msec;

	if (!buf || buflen < 12U) {
		return 0;
	}

	t_ms = wb_time_get_unix_ms();
	sec = (uint32_t)(t_ms / 1000ULL);
	msec = (uint32_t)(t_ms % 1000ULL);

	if (s_synced) {
		int y, mo, d, hh, mm, ss;
		unix_to_ymdhms(sec, &y, &mo, &d, &hh, &mm, &ss);
		return snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d.%03u",
				y, mo, d, hh, mm, ss, (unsigned)msec);
	}

	return snprintf(buf, buflen, "T+%06u.%03u", (unsigned)sec,
			(unsigned)msec);
}
