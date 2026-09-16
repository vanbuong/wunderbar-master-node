/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: MIT
 *
 * Zephyr backend for wb_time: wall clock is SYS_CLOCK_REALTIME so the
 * subsystem logging timestamps (CONFIG_LOG_TIMESTAMP_USE_REALTIME) match.
 */

#include "wb_time.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/clock.h>

#include <stdio.h>
#include <stdint.h>

/* Same civil-time threshold as GS GETTIME plausibility (~2020). */
#define WB_TIME_SYNCED_MIN_UNIX 1577836800UL

void wb_time_init(wb_time_millis_fn millis, void *ctx)
{
	/* Monotonic source is Zephyr uptime; millis callback unused. */
	(void)millis;
	(void)ctx;
}

bool wb_time_is_synced(void)
{
	return wb_time_get_unix() >= WB_TIME_SYNCED_MIN_UNIX;
}

void wb_time_set_unix(uint32_t unix_sec)
{
	struct timespec ts = {
		.tv_sec = (time_t)unix_sec,
		.tv_nsec = 0,
	};

	(void)sys_clock_settime(SYS_CLOCK_REALTIME, &ts);
}

uint32_t wb_time_get_unix(void)
{
	struct timespec ts;

	if (sys_clock_gettime(SYS_CLOCK_REALTIME, &ts) != 0) {
		return 0U;
	}
	if (ts.tv_sec < 0) {
		return 0U;
	}
	return (uint32_t)ts.tv_sec;
}

uint64_t wb_time_get_unix_ms(void)
{
	struct timespec ts;

	if (sys_clock_gettime(SYS_CLOCK_REALTIME, &ts) != 0) {
		return (uint64_t)k_uptime_get();
	}
	return ((uint64_t)ts.tv_sec * 1000ULL) +
	       ((uint64_t)ts.tv_nsec / 1000000ULL);
}

static void unix_to_ymdhms(uint32_t unix_sec, int *y, int *mo, int *d,
			   int *hh, int *mm, int *ss)
{
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
	uint64_t t_ms = wb_time_get_unix_ms();
	uint32_t sec = (uint32_t)(t_ms / 1000ULL);
	uint32_t msec = (uint32_t)(t_ms % 1000ULL);

	if (!buf || buflen < 12U) {
		return 0;
	}

	if (wb_time_is_synced()) {
		int y, mo, d, hh, mm, ss;

		unix_to_ymdhms(sec, &y, &mo, &d, &hh, &mm, &ss);
		return snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d.%03u",
				y, mo, d, hh, mm, ss, (unsigned)msec);
	}

	return snprintf(buf, buflen, "T+%06u.%03u", (unsigned)sec,
			(unsigned)msec);
}
