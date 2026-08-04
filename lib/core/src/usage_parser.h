#pragma once
#include "usage_types.h"

// 解析 Kimi /coding/v1/usages 响应体为 UsageData。
// 成功返回 PARSE_OK 并写入 *out；失败返回对应 ParseResult，out 不被修改。
ParseResult parse_usage_json(const char* json, UsageData* out);
