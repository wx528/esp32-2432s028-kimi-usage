#include "display_model.h"

int usage_percent(long used, long limit) {
  if (limit <= 0) return 0;
  if (used < 0) used = 0;
  if (used > limit) used = limit;
  return (int)((used * 100L) / limit);
}

UsageLevel usage_level(int percent) {
  if (percent < 70) return LEVEL_NORMAL;
  if (percent <= 90) return LEVEL_WARNING;
  return LEVEL_CRITICAL;
}
