#include <unity.h>
#include "minimax_parser.h"

static const char* SAMPLE = R"json({
  "model_remains": [
    {
      "start_time": 1785758400000,
      "end_time": 1785772800000,
      "remains_time": 5761296,
      "current_interval_total_count": 0,
      "current_interval_usage_count": 0,
      "model_name": "general",
      "current_weekly_total_count": 0,
      "current_weekly_usage_count": 0,
      "weekly_start_time": 1785686400000,
      "weekly_end_time": 1786291200000,
      "weekly_remains_time": 524161296,
      "current_interval_status": 1,
      "current_weekly_status": 1,
      "current_interval_remaining_percent": 98,
      "current_weekly_remaining_percent": 87,
      "weekly_boost_permille": 1500
    }
  ],
  "base_resp": {"status_code": 0, "status_msg": "success"}
})json";

void test_parse_percent_fields() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_minimax_json(SAMPLE, &d));
  TEST_ASSERT_EQUAL_INT32(13, d.plan_used);      // 100 - 87
  TEST_ASSERT_EQUAL_INT32(100, d.plan_limit);
  TEST_ASSERT_EQUAL_UINT32(1786291200UL, d.plan_reset);
  TEST_ASSERT_EQUAL_INT32(2, d.window_used);     // 100 - 98
  TEST_ASSERT_EQUAL_INT32(100, d.window_limit);
  TEST_ASSERT_EQUAL_UINT32(1785772800UL, d.window_reset);
}

void test_counts_are_ignored() {
  // count 字段即使非 0 也不影响结果（MiniMax 转 token 计量后恒 0，percent 才权威）
  const char* j = R"json({
    "model_remains":[{"model_name":"general","current_weekly_total_count":999,
      "current_weekly_usage_count":999,"current_interval_total_count":999,"current_interval_usage_count":999,
      "current_weekly_remaining_percent":40,"current_interval_remaining_percent":60,
      "weekly_end_time":1786291200000,"end_time":1785772800000}],
    "base_resp":{"status_code":0,"status_msg":"success"}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_minimax_json(j, &d));
  TEST_ASSERT_EQUAL_INT32(60, d.plan_used);
  TEST_ASSERT_EQUAL_INT32(40, d.window_used);
}

void test_base_resp_error_is_api_error() {
  const char* j = R"json({"model_remains":[],"base_resp":{"status_code":1,"status_msg":"unauthorized"}})json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_API, parse_minimax_json(j, &d));
}

void test_missing_general_entry_is_missing() {
  const char* j = R"json({"model_remains":[{"model_name":"video"}],"base_resp":{"status_code":0}})json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_MISSING, parse_minimax_json(j, &d));
}

void test_invalid_json_is_json_error() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parse_minimax_json("not json", &d));
}

void test_percent_out_of_range_is_bad_value() {
  const char* j = R"json({
    "model_remains":[{"model_name":"general","current_weekly_remaining_percent":150,
      "current_interval_remaining_percent":50,"weekly_end_time":1786291200000,"end_time":1785772800000}],
    "base_resp":{"status_code":0}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_BAD_VALUE, parse_minimax_json(j, &d));
}

void test_zero_reset_is_bad_value() {
  const char* j = R"json({
    "model_remains":[{"model_name":"general","current_weekly_remaining_percent":50,
      "current_interval_remaining_percent":50,"weekly_end_time":0,"end_time":1785772800000}],
    "base_resp":{"status_code":0}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_BAD_VALUE, parse_minimax_json(j, &d));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_parse_percent_fields);
  RUN_TEST(test_counts_are_ignored);
  RUN_TEST(test_base_resp_error_is_api_error);
  RUN_TEST(test_missing_general_entry_is_missing);
  RUN_TEST(test_invalid_json_is_json_error);
  RUN_TEST(test_percent_out_of_range_is_bad_value);
  RUN_TEST(test_zero_reset_is_bad_value);
  return UNITY_END();
}
