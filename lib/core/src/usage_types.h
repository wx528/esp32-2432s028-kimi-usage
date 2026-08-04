#pragma once
#include <stdint.h>

// API 取回、解析后的结构化数据。reset 均为 UTC epoch 秒。
struct UsageData {
  long plan_used;      // 周额度已用
  long plan_limit;     // 周额度上限
  uint32_t plan_reset; // 周额度重置时刻
  long window_used;    // 5 小时窗口已用
  long window_limit;   // 5 小时窗口上限
  uint32_t window_reset;
};

enum ParseResult : uint8_t {
  PARSE_OK = 0,
  PARSE_ERR_JSON,       // 不是合法 JSON
  PARSE_ERR_MISSING,    // 缺少必要字段
  PARSE_ERR_BAD_VALUE,  // 字段值类型/格式不对
  PARSE_ERR_KEY_DISABLED // boosterWallet.status == STATUS_DISABLED
};
