#include <unity.h>
#include <string.h>
#include "format_utils.h"

void test_format_thousands() {
  char buf[24];
  TEST_ASSERT_EQUAL_STRING("0", format_thousands(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("60", format_thousands(60, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1,360", format_thousands(1360, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1,234,567", format_thousands(1234567, buf, sizeof(buf)));
}

void test_format_countdown_days_hours_minutes() {
  char buf[32];
  TEST_ASSERT_EQUAL_STRING("resets in 3d", format_countdown(3L*86400 + 3600, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resets in 5h", format_countdown(5L*3600 + 1800, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resets in 42m", format_countdown(42L*60 + 30, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resets in 1m", format_countdown(30, buf, sizeof(buf)));
}

void test_format_countdown_past_is_resetting() {
  char buf[32];
  TEST_ASSERT_EQUAL_STRING("resetting", format_countdown(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resetting", format_countdown(-60, buf, sizeof(buf)));
}

void test_format_age() {
  char buf[24];
  TEST_ASSERT_EQUAL_STRING("just now", format_age(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("just now", format_age(59, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("5m ago", format_age(300, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1h ago", format_age(3600, buf, sizeof(buf)));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_format_thousands);
  RUN_TEST(test_format_countdown_days_hours_minutes);
  RUN_TEST(test_format_countdown_past_is_resetting);
  RUN_TEST(test_format_age);
  return UNITY_END();
}
