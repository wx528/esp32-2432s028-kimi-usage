#include <unity.h>
#include <string.h>
#include "provider.h"

static const char* KIMI_JSON = R"json({
  "usage": {"limit":"100","used":"89","resetTime":"2026-07-21T06:38:42Z"},
  "limits":[{"detail":{"limit":"100","used":"7","resetTime":"2026-07-17T20:38:42Z"}}]
})json";

static const char* MINIMAX_JSON = R"json({
  "model_remains":[{"model_name":"general","current_weekly_remaining_percent":87,
    "current_interval_remaining_percent":98,"weekly_end_time":1786291200000,"end_time":1785772800000}],
  "base_resp":{"status_code":0,"status_msg":"success"}
})json";

void test_provider_names_and_urls() {
  TEST_ASSERT_EQUAL_STRING("KIMI", provider_name(PROVIDER_KIMI));
  TEST_ASSERT_EQUAL_STRING("MINIMAX", provider_name(PROVIDER_MINIMAX));
  TEST_ASSERT_EQUAL_STRING("https://api.kimi.com/coding/v1/usages", provider_url(PROVIDER_KIMI));
  TEST_ASSERT_EQUAL_STRING("https://www.minimaxi.com/v1/token_plan/remains", provider_url(PROVIDER_MINIMAX));
}

void test_parse_dispatches_to_kimi() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, provider_parse(PROVIDER_KIMI, KIMI_JSON, &d));
  TEST_ASSERT_EQUAL_INT32(89, d.plan_used);
}

void test_parse_dispatches_to_minimax() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, provider_parse(PROVIDER_MINIMAX, MINIMAX_JSON, &d));
  TEST_ASSERT_EQUAL_INT32(13, d.plan_used);
}

void test_cross_parse_fails_cleanly() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_MISSING, provider_parse(PROVIDER_KIMI, MINIMAX_JSON, &d));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_provider_names_and_urls);
  RUN_TEST(test_parse_dispatches_to_kimi);
  RUN_TEST(test_parse_dispatches_to_minimax);
  RUN_TEST(test_cross_parse_fails_cleanly);
  return UNITY_END();
}
