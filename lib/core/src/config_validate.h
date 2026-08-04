#pragma once
#include <stdint.h>

enum ProviderMode : uint8_t {
  MODE_KIMI = 0,
  MODE_MINIMAX,
  MODE_BOTH
};

struct DeviceConfig {
  char ssid[64];
  char password[64];
  char api_key[128];      // Kimi
  char minimax_key[128];  // MiniMax
  long refresh_interval;  // 秒，30-3600
  uint8_t provider_mode;  // ProviderMode
};

enum ConfigError : uint8_t {
  CFG_OK = 0,
  CFG_ERR_NO_SSID,
  CFG_ERR_NO_KEY,
  CFG_ERR_BAD_INTERVAL,
  CFG_ERR_BAD_MODE
};

ConfigError validate_config(const DeviceConfig* cfg);

const char* mask_api_key(const char* key, char* buf, int buf_size);
