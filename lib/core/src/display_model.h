#pragma once
#include <stdint.h>

enum UsageLevel : uint8_t {
  LEVEL_NORMAL = 0,
  LEVEL_WARNING,
  LEVEL_CRITICAL
};

int usage_percent(long used, long limit);

UsageLevel usage_level(int percent);
