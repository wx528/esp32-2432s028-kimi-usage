#include "time_parse.h"
#include <string.h>
#include <stdlib.h>

// Howard Hinnant 的 days_from_civil：公历年月日 → 自 1970-01-01 的天数
static long days_from_civil(long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);            // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
  return era * 146097L + (long)doe - 719468L;
}

static int take_digits(const char*& p, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (*p < '0' || *p > '9') return -1;
    v = v * 10 + (*p - '0');
    p++;
  }
  return v;
}

uint32_t parse_iso8601_epoch(const char* s) {
  if (!s || strlen(s) < 20) return 0;
  const char* p = s;
  int y = take_digits(p, 4); if (y < 0 || *p != '-') return 0; p++;
  int mo = take_digits(p, 2); if (mo < 1 || mo > 12 || *p != '-') return 0; p++;
  int d = take_digits(p, 2); if (d < 1 || d > 31 || *p != 'T') return 0; p++;
  int h = take_digits(p, 2); if (h < 0 || h > 23 || *p != ':') return 0; p++;
  int mi = take_digits(p, 2); if (mi < 0 || mi > 59 || *p != ':') return 0; p++;
  int sec = take_digits(p, 2); if (sec < 0 || sec > 59) return 0;

  long days = days_from_civil(y, (unsigned)mo, (unsigned)d);
  long secs = days * 86400L + (long)h * 3600L + (long)mi * 60L + sec;
  if (secs < 0) return 0;
  return (uint32_t)secs;
}
