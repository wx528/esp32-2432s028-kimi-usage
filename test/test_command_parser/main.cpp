#include <unity.h>
#include <string.h>
#include "command_parser.h"

void test_no_arg_commands() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("GET:CONFIG", &c));
  TEST_ASSERT_EQUAL(CMD_GET_CONFIG, c.type);
  TEST_ASSERT_TRUE(parse_command("GET:USAGE", &c));
  TEST_ASSERT_EQUAL(CMD_GET_USAGE, c.type);
  TEST_ASSERT_TRUE(parse_command("REFRESH", &c));
  TEST_ASSERT_EQUAL(CMD_REFRESH, c.type);
  TEST_ASSERT_TRUE(parse_command("RESET:CONFIG", &c));
  TEST_ASSERT_EQUAL(CMD_RESET_CONFIG, c.type);
  TEST_ASSERT_TRUE(parse_command("REBOOT", &c));
  TEST_ASSERT_EQUAL(CMD_REBOOT, c.type);
}

void test_set_interval_valid_and_bounds() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:INTERVAL:60", &c));
  TEST_ASSERT_EQUAL(CMD_SET_INTERVAL, c.type);
  TEST_ASSERT_EQUAL_INT32(60, c.interval);

  TEST_ASSERT_FALSE(parse_command("SET:INTERVAL:29", &c));    // <30 非法
  TEST_ASSERT_FALSE(parse_command("SET:INTERVAL:3601", &c));  // >3600 非法
  TEST_ASSERT_FALSE(parse_command("SET:INTERVAL:abc", &c));   // 非数字非法
}

void test_set_key() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:KEY:sk-kimi-abcdef123456", &c));
  TEST_ASSERT_EQUAL(CMD_SET_KEY, c.type);
  TEST_ASSERT_EQUAL_STRING("sk-kimi-abcdef123456", c.key);
  TEST_ASSERT_FALSE(parse_command("SET:KEY:", &c)); // 空 key 非法
}

void test_set_wifi_password_may_contain_colons() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:WIFI:HomeWiFi:p@ss:w0rd", &c));
  TEST_ASSERT_EQUAL(CMD_SET_WIFI, c.type);
  TEST_ASSERT_EQUAL_STRING("HomeWiFi", c.ssid);
  TEST_ASSERT_EQUAL_STRING("p@ss:w0rd", c.password); // 密码按"剩余全部"取
}

void test_set_wifi_open_network_empty_password_ok() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:WIFI:CafeNet:", &c));
  TEST_ASSERT_EQUAL(CMD_SET_WIFI, c.type);
  TEST_ASSERT_EQUAL_STRING("CafeNet", c.ssid);
  TEST_ASSERT_EQUAL_STRING("", c.password);
}

void test_unknown_and_malformed() {
  Command c;
  TEST_ASSERT_FALSE(parse_command("FOOBAR", &c));
  TEST_ASSERT_FALSE(parse_command("", &c));
  TEST_ASSERT_FALSE(parse_command("SET:WIFI:onlyssid", &c)); // 缺密码段
}

void test_set_provider() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:PROVIDER:kimi", &c));
  TEST_ASSERT_EQUAL(CMD_SET_PROVIDER, c.type);
  TEST_ASSERT_EQUAL(MODE_KIMI, c.provider_mode);
  TEST_ASSERT_TRUE(parse_command("SET:PROVIDER:minimax", &c));
  TEST_ASSERT_EQUAL(MODE_MINIMAX, c.provider_mode);
  TEST_ASSERT_TRUE(parse_command("SET:PROVIDER:both", &c));
  TEST_ASSERT_EQUAL(MODE_BOTH, c.provider_mode);
  TEST_ASSERT_FALSE(parse_command("SET:PROVIDER:openai", &c));
  TEST_ASSERT_FALSE(parse_command("SET:PROVIDER:", &c));
}

void test_set_mmkey() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:MMKEY:mm-abcdef123456", &c));
  TEST_ASSERT_EQUAL(CMD_SET_MMKEY, c.type);
  TEST_ASSERT_EQUAL_STRING("mm-abcdef123456", c.mmkey);
  TEST_ASSERT_FALSE(parse_command("SET:MMKEY:", &c));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_no_arg_commands);
  RUN_TEST(test_set_interval_valid_and_bounds);
  RUN_TEST(test_set_key);
  RUN_TEST(test_set_wifi_password_may_contain_colons);
  RUN_TEST(test_set_wifi_open_network_empty_password_ok);
  RUN_TEST(test_unknown_and_malformed);
  RUN_TEST(test_set_provider);
  RUN_TEST(test_set_mmkey);
  return UNITY_END();
}
