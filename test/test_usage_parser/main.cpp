#include <unity.h>
#include <string.h>
#include "usage_parser.h"

static const char* SAMPLE = R"json({
  "usage": {"limit":"100","used":"89","remaining":"11","resetTime":"2026-07-21T06:38:42.676140Z"},
  "limits":[
    {"window":{"duration":300,"timeUnit":"TIME_UNIT_MINUTE"},
     "detail":{"limit":"100","used":"7","remaining":"93","resetTime":"2026-07-17T20:38:42.676140Z"}}
  ]
})json";

void test_parse_extracts_all_fields() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_usage_json(SAMPLE, &d));
  TEST_ASSERT_EQUAL_INT32(89, d.plan_used);
  TEST_ASSERT_EQUAL_INT32(100, d.plan_limit);
  TEST_ASSERT_EQUAL_UINT32(1784615922UL, d.plan_reset);
  TEST_ASSERT_EQUAL_INT32(7, d.window_used);
  TEST_ASSERT_EQUAL_INT32(100, d.window_limit);
  TEST_ASSERT_EQUAL_UINT32(1784320722UL, d.window_reset);
}

void test_computes_used_from_remaining_when_used_absent() {
  const char* j = R"json({
    "usage":{"limit":"100","remaining":"11","resetTime":"2026-07-21T06:38:42Z"},
    "limits":[{"detail":{"limit":"100","remaining":"93","resetTime":"2026-07-17T20:38:42Z"}}]
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_usage_json(j, &d));
  TEST_ASSERT_EQUAL_INT32(89, d.plan_used);
  TEST_ASSERT_EQUAL_INT32(7, d.window_used);
}

void test_missing_detail_is_missing_error() {
  const char* j = R"json({"usage":{"limit":"100","used":"1","resetTime":"2026-07-21T00:00:00Z"}})json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_MISSING, parse_usage_json(j, &d));
}

void test_invalid_json_is_json_error() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parse_usage_json("not json", &d));
}

void test_disabled_booster_wallet_is_key_disabled() {
  const char* j = R"json({
    "usage":{"limit":"100","remaining":"100","resetTime":"2026-07-28T06:38:42Z"},
    "limits":[{"detail":{"limit":"100","remaining":"100","resetTime":"2026-07-23T11:38:42Z"}}],
    "boosterWallet":{"status":"STATUS_DISABLED"}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_KEY_DISABLED, parse_usage_json(j, &d));
}

void test_enabled_booster_wallet_parses_fine() {
  const char* j = R"json({
    "usage":{"limit":"100","used":"10","resetTime":"2026-07-28T06:38:42Z"},
    "limits":[{"detail":{"limit":"100","used":"5","resetTime":"2026-07-23T11:38:42Z"}}],
    "boosterWallet":{"status":"STATUS_ENABLED"}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_usage_json(j, &d));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_parse_extracts_all_fields);
  RUN_TEST(test_computes_used_from_remaining_when_used_absent);
  RUN_TEST(test_missing_detail_is_missing_error);
  RUN_TEST(test_invalid_json_is_json_error);
  RUN_TEST(test_disabled_booster_wallet_is_key_disabled);
  RUN_TEST(test_enabled_booster_wallet_parses_fine);
  return UNITY_END();
}
