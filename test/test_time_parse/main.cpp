#include <unity.h>
#include "time_parse.h"

void test_full_iso8601_with_fraction_and_z() {
  TEST_ASSERT_TRUE(parse_iso8601_epoch("2026-07-21T06:38:42.676140Z") != 0);
  // 与已知 epoch 对照：2026-07-21T06:38:42Z
  TEST_ASSERT_EQUAL_UINT32(1784615922UL, parse_iso8601_epoch("2026-07-21T06:38:42.676140Z"));
}

void test_iso8601_without_fraction() {
  TEST_ASSERT_EQUAL_UINT32(1784615922UL, parse_iso8601_epoch("2026-07-21T06:38:42Z"));
}

void test_epoch_boundary() {
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch("1970-01-01T00:00:00Z"));
}

void test_garbage_returns_zero() {
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch(""));
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch("not a date"));
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch("2026-07-21"));
}

void test_ms_epoch_to_sec() {
  TEST_ASSERT_EQUAL_UINT32(1785772800UL, ms_epoch_to_sec(1785772800000LL));
  TEST_ASSERT_EQUAL_UINT32(1786291200UL, ms_epoch_to_sec(1786291200000LL));
}

void test_ms_epoch_to_sec_nonpositive_is_zero() {
  TEST_ASSERT_EQUAL_UINT32(0UL, ms_epoch_to_sec(0));
  TEST_ASSERT_EQUAL_UINT32(0UL, ms_epoch_to_sec(-5));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_full_iso8601_with_fraction_and_z);
  RUN_TEST(test_iso8601_without_fraction);
  RUN_TEST(test_epoch_boundary);
  RUN_TEST(test_garbage_returns_zero);
  RUN_TEST(test_ms_epoch_to_sec);
  RUN_TEST(test_ms_epoch_to_sec_nonpositive_is_zero);
  return UNITY_END();
}
