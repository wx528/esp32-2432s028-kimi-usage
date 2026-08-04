#include <unity.h>
#include <string.h>
#include "config_validate.h"

void test_valid_config() {
  DeviceConfig c;
  strcpy(c.ssid, "HomeWiFi");
  strcpy(c.password, "secret");
  strcpy(c.api_key, "sk-kimi-abcdef1234567890");
  c.refresh_interval = 60;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
}

void test_empty_ssid_or_key_invalid() {
  DeviceConfig c;
  strcpy(c.ssid, "");
  strcpy(c.password, "x");
  strcpy(c.api_key, "sk-abc");
  c.refresh_interval = 60;
  TEST_ASSERT_EQUAL(CFG_ERR_NO_SSID, validate_config(&c));

  strcpy(c.ssid, "ok");
  c.api_key[0] = '\0';
  TEST_ASSERT_EQUAL(CFG_ERR_NO_KEY, validate_config(&c));
}

void test_interval_bounds() {
  DeviceConfig c;
  strcpy(c.ssid, "s"); strcpy(c.api_key, "k");
  c.refresh_interval = 29;
  TEST_ASSERT_EQUAL(CFG_ERR_BAD_INTERVAL, validate_config(&c));
  c.refresh_interval = 3601;
  TEST_ASSERT_EQUAL(CFG_ERR_BAD_INTERVAL, validate_config(&c));
  c.refresh_interval = 30;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
  c.refresh_interval = 3600;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
}

void test_open_network_password_optional() {
  DeviceConfig c;
  strcpy(c.ssid, "CafeNet");
  c.password[0] = '\0';
  strcpy(c.api_key, "k");
  c.refresh_interval = 60;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
}

void test_mask_api_key() {
  char buf[32];
  mask_api_key("sk-kimi-abcdef1234567890", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("sk-k...7890", buf);
  mask_api_key("short", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("****", buf);
  mask_api_key("", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("(unset)", buf);
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_valid_config);
  RUN_TEST(test_empty_ssid_or_key_invalid);
  RUN_TEST(test_interval_bounds);
  RUN_TEST(test_open_network_password_optional);
  RUN_TEST(test_mask_api_key);
  return UNITY_END();
}
