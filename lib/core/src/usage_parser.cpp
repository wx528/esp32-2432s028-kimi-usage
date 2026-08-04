#include "usage_parser.h"
#include "time_parse.h"
#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>

// 从 JsonVariant 取整数：优先 as<long>()；当值是字符串且非空时兜底 strtol
static bool json_to_long(JsonVariantConst v, long* out) {
  if (v.isNull()) return false;
  if (v.is<long>() || v.is<int>() || v.is<float>() || v.is<double>()) {
    *out = (long)v.as<double>();
    return true;
  }
  const char* s = v.as<const char*>();
  if (s && *s) {
    char* end = nullptr;
    long val = strtol(s, &end, 10);
    if (end != s) { *out = val; return true; }
  }
  return false;
}

ParseResult parse_usage_json(const char* json, UsageData* out) {
  if (!json || !out) return PARSE_ERR_MISSING;

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return PARSE_ERR_JSON;

  JsonVariantConst bw = doc["boosterWallet"];
  if (!bw.isNull()) {
    const char* st = bw["status"] | "";
    if (strcmp(st, "STATUS_DISABLED") == 0) return PARSE_ERR_KEY_DISABLED;
  }

  JsonVariantConst usage = doc["usage"];
  JsonVariantConst detail = doc["limits"][0]["detail"];
  if (usage.isNull() || detail.isNull()) return PARSE_ERR_MISSING;

  long plan_limit, plan_used, win_limit, win_used;
  if (!json_to_long(usage["limit"], &plan_limit)) return PARSE_ERR_BAD_VALUE;
  if (!json_to_long(detail["limit"], &win_limit)) return PARSE_ERR_BAD_VALUE;

  // used 优先取 "used"，缺失时用 limit - remaining
  long tmp;
  if (json_to_long(usage["used"], &tmp)) plan_used = tmp;
  else if (json_to_long(usage["remaining"], &tmp)) plan_used = plan_limit - tmp;
  else return PARSE_ERR_BAD_VALUE;

  if (json_to_long(detail["used"], &tmp)) win_used = tmp;
  else if (json_to_long(detail["remaining"], &tmp)) win_used = win_limit - tmp;
  else return PARSE_ERR_BAD_VALUE;

  const char* plan_reset_s = usage["resetTime"] | "";
  const char* win_reset_s = detail["resetTime"] | "";
  uint32_t plan_reset = parse_iso8601_epoch(plan_reset_s);
  uint32_t win_reset = parse_iso8601_epoch(win_reset_s);
  if (plan_reset == 0 || win_reset == 0) return PARSE_ERR_BAD_VALUE;

  out->plan_used = plan_used;
  out->plan_limit = plan_limit;
  out->plan_reset = plan_reset;
  out->window_used = win_used;
  out->window_limit = win_limit;
  out->window_reset = win_reset;
  return PARSE_OK;
}
