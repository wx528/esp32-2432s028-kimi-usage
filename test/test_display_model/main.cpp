#include <unity.h>
#include "display_model.h"

void test_usage_percent_clamps_0_100() {
  TEST_ASSERT_EQUAL_INT(0, usage_percent(0, 2000));
  TEST_ASSERT_EQUAL_INT(50, usage_percent(1000, 2000));
  TEST_ASSERT_EQUAL_INT(100, usage_percent(2000, 2000));
  TEST_ASSERT_EQUAL_INT(100, usage_percent(3000, 2000));
  TEST_ASSERT_EQUAL_INT(0, usage_percent(-1, 2000));
  TEST_ASSERT_EQUAL_INT(0, usage_percent(100, 0));
}

void test_level_thresholds() {
  TEST_ASSERT_EQUAL(LEVEL_NORMAL,   usage_level(69));
  TEST_ASSERT_EQUAL(LEVEL_NORMAL,   usage_level(0));
  TEST_ASSERT_EQUAL(LEVEL_WARNING,  usage_level(70));
  TEST_ASSERT_EQUAL(LEVEL_WARNING,  usage_level(90));
  TEST_ASSERT_EQUAL(LEVEL_CRITICAL, usage_level(91));
  TEST_ASSERT_EQUAL(LEVEL_CRITICAL, usage_level(100));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_usage_percent_clamps_0_100);
  RUN_TEST(test_level_thresholds);
  return UNITY_END();
}
