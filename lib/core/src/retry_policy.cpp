#include "retry_policy.h"

long retry_interval_sec(long base_sec, int failures) {
  long interval = base_sec;
  for (int i = 0; i < failures && interval < 300; i++) {
    interval *= 2;
    if (interval > 300) interval = 300;
  }
  if (interval > 300) interval = 300;
  return interval;
}
