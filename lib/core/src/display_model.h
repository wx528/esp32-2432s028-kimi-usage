#pragma once
#include <stdint.h>

enum UsageLevel : uint8_t {
  LEVEL_NORMAL = 0,
  LEVEL_WARNING,
  LEVEL_CRITICAL
};

int usage_percent(long used, long limit);

UsageLevel usage_level(int percent);

// 显示方向循环：0→1→2→3→0；非法输入（>=3 以外）归 0
uint8_t rotation_next(uint8_t r);
