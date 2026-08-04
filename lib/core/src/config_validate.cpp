#include "config_validate.h"
#include <string.h>
#include <stdio.h>

ConfigError validate_config(const DeviceConfig* cfg) {
  if (!cfg) return CFG_ERR_NO_SSID;
  if (cfg->ssid[0] == '\0') return CFG_ERR_NO_SSID;
  if (cfg->api_key[0] == '\0') return CFG_ERR_NO_KEY;
  if (cfg->refresh_interval < 30 || cfg->refresh_interval > 3600) return CFG_ERR_BAD_INTERVAL;
  return CFG_OK;
}

const char* mask_api_key(const char* key, char* buf, int buf_size) {
  if (!key || !*key) {
    snprintf(buf, buf_size, "(unset)");
    return buf;
  }
  int len = (int)strlen(key);
  if (len <= 8) {
    snprintf(buf, buf_size, "****");
    return buf;
  }
  snprintf(buf, buf_size, "%.4s...%s", key, key + len - 4);
  return buf;
}
