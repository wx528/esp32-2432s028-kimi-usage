#include "format_utils.h"
#include <stdio.h>
#include <stdlib.h>

const char* format_thousands(long value, char* buf, int buf_size) {
  char digits[24];
  snprintf(digits, sizeof(digits), "%ld", labs(value));
  int len = 0; while (digits[len]) len++;

  int out = 0;
  if (value < 0 && out < buf_size - 1) buf[out++] = '-';
  for (int i = 0; i < len && out < buf_size - 1; i++) {
    if (i > 0 && (len - i) % 3 == 0 && out < buf_size - 1) buf[out++] = ',';
    buf[out++] = digits[i];
  }
  buf[out] = '\0';
  return buf;
}

const char* format_countdown(long seconds, char* buf, int buf_size) {
  if (seconds <= 0) {
    snprintf(buf, buf_size, "resetting");
    return buf;
  }
  long minutes = seconds / 60;
  if (minutes < 60) {
    snprintf(buf, buf_size, "resets in %ldm", minutes < 1 ? 1 : minutes);
  } else if (minutes < 1440) {
    snprintf(buf, buf_size, "resets in %ldh", minutes / 60);
  } else {
    snprintf(buf, buf_size, "resets in %ldd", minutes / 1440);
  }
  return buf;
}

const char* format_age(long seconds, char* buf, int buf_size) {
  if (seconds < 60) {
    snprintf(buf, buf_size, "just now");
  } else if (seconds < 3600) {
    snprintf(buf, buf_size, "%ldm ago", seconds / 60);
  } else {
    snprintf(buf, buf_size, "%ldh ago", seconds / 3600);
  }
  return buf;
}
