#pragma once
#include <stdint.h>
#include "usage_types.h"

enum Provider : uint8_t {
  PROVIDER_KIMI = 0,
  PROVIDER_MINIMAX
};

const char* provider_name(Provider p);  // "KIMI" / "MINIMAX"，屏幕标题用
const char* provider_url(Provider p);   // API endpoint，net 层用

// 按 provider 分发到对应 parser。
ParseResult provider_parse(Provider p, const char* json, UsageData* out);
