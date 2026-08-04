#include <unity.h>
#include "retry_policy.h"

void test_backoff_doubles_then_caps() {
  TEST_ASSERT_EQUAL_INT32(60, retry_interval_sec(60, 0));
  TEST_ASSERT_EQUAL_INT32(120, retry_interval_sec(60, 1));
  TEST_ASSERT_EQUAL_INT32(240, retry_interval_sec(60, 2));
  TEST_ASSERT_EQUAL_INT32(300, retry_interval_sec(60, 3)); // 封顶 300
  TEST_ASSERT_EQUAL_INT32(300, retry_interval_sec(60, 10));// 再高也封顶
}

void test_backoff_respects_base_and_zero_failures() {
  TEST_ASSERT_EQUAL_INT32(10, retry_interval_sec(10, 0));
  TEST_ASSERT_EQUAL_INT32(20, retry_interval_sec(10, 1));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_backoff_doubles_then_caps);
  RUN_TEST(test_backoff_respects_base_and_zero_failures);
  return UNITY_END();
}
