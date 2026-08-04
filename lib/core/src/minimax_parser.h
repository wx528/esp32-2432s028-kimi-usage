#pragma once
#include "usage_types.h"

// 解析 MiniMax /v1/token_plan/remains 响应体为 UsageData。
// 取 model_remains[] 中 model_name=="general" 的条目；用量以 remaining_percent 为准
//（count 字段自 MiniMax 转 token 计量后恒 0），limit 恒 100；重置时间为毫秒 epoch。
// 成功返回 PARSE_OK 并写入 *out；失败返回对应 ParseResult，out 不被修改。
ParseResult parse_minimax_json(const char* json, UsageData* out);
