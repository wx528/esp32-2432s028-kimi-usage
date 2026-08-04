#pragma once
#include <stdint.h>

struct DeviceConfig {
  char ssid[64];
  char password[64];
  char api_key[128];
  long refresh_interval;
};

enum ConfigError : uint8_t {
  CFG_OK = 0,
  CFG_ERR_NO_SSID,
  CFG_ERR_NO_KEY,
  CFG_ERR_BAD_INTERVAL
};

ConfigError validate_config(const DeviceConfig* cfg);

const char* mask_api_key(const char* key, char* buf, int buf_size);
