#pragma once
#include <stdint.h>
#include "config_validate.h"

enum CommandType : uint8_t {
  CMD_UNKNOWN = 0,
  CMD_GET_CONFIG,
  CMD_SET_WIFI,
  CMD_SET_KEY,
  CMD_SET_INTERVAL,
  CMD_REFRESH,
  CMD_GET_USAGE,
  CMD_RESET_CONFIG,
  CMD_REBOOT,
  CMD_SET_PROVIDER,
  CMD_SET_MMKEY
};

struct Command {
  CommandType type;
  char ssid[64];
  char password[64];
  char key[128];
  char mmkey[128];
  long interval;
  uint8_t provider_mode; // ProviderMode，仅 CMD_SET_PROVIDER 有效
};

// 解析一行命令（不含换行）。合法返回 true 并填 *out，非法返回 false。
bool parse_command(const char* line, Command* out);
