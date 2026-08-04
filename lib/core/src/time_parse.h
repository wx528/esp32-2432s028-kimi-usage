#pragma once
#include <stdint.h>

// 解析形如 "2026-07-21T06:38:42.676140Z" 的 ISO8601（UTC）为 epoch 秒。
// 解析失败返回 0。不含时区转换，输入一律按 UTC 处理。
uint32_t parse_iso8601_epoch(const char* s);

// 毫秒 epoch（UTC）转秒。<=0 返回 0。
uint32_t ms_epoch_to_sec(int64_t ms);
