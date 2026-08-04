#include "minimax_parser.h"
#include "time_parse.h"
#include <ArduinoJson.h>
#include <string.h>

ParseResult parse_minimax_json(const char* json, UsageData* out) {
  if (!json || !out) return PARSE_ERR_MISSING;

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return PARSE_ERR_JSON;

  JsonVariantConst br = doc["base_resp"];
  if (!br.isNull() && br["status_code"].as<long>() != 0) return PARSE_ERR_API;

  JsonArrayConst arr = doc["model_remains"].as<JsonArrayConst>();
  if (arr.isNull()) return PARSE_ERR_MISSING;

  JsonVariantConst general;
  for (JsonVariantConst item : arr) {
    if (strcmp(item["model_name"] | "", "general") == 0) { general = item; break; }
  }
  if (general.isNull()) return PARSE_ERR_MISSING;

  if (!general["current_weekly_remaining_percent"].is<long>()) return PARSE_ERR_BAD_VALUE;
  if (!general["current_interval_remaining_percent"].is<long>()) return PARSE_ERR_BAD_VALUE;
  long wp = general["current_weekly_remaining_percent"].as<long>();
  long ip = general["current_interval_remaining_percent"].as<long>();
  if (wp < 0 || wp > 100 || ip < 0 || ip > 100) return PARSE_ERR_BAD_VALUE;

  uint32_t plan_reset = ms_epoch_to_sec(general["weekly_end_time"].as<int64_t>());
  uint32_t win_reset = ms_epoch_to_sec(general["end_time"].as<int64_t>());
  if (plan_reset == 0 || win_reset == 0) return PARSE_ERR_BAD_VALUE;

  out->plan_used = 100 - wp;
  out->plan_limit = 100;
  out->plan_reset = plan_reset;
  out->window_used = 100 - ip;
  out->window_limit = 100;
  out->window_reset = win_reset;
  return PARSE_OK;
}
