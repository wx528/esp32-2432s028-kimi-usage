#include "config_store.h"
#include <Preferences.h>

static const char* NS = "cydkimi";

bool config_store_load(DeviceConfig* cfg) {
  if (!cfg) return false;
  Preferences p;
  if (!p.begin(NS, true)) return false;
  String ssid = p.getString("ssid", "");
  String pass = p.getString("pass", "");
  String key = p.getString("key", "");
  long interval = p.getLong("interval", 60);
  String mmkey = p.getString("mmkey", "");
  uint8_t mode = p.getUChar("mode", MODE_KIMI);
  uint8_t rot = p.getUChar("rot", 0);
  p.end();

  strncpy(cfg->ssid, ssid.c_str(), sizeof(cfg->ssid) - 1);
  cfg->ssid[sizeof(cfg->ssid) - 1] = '\0';
  strncpy(cfg->password, pass.c_str(), sizeof(cfg->password) - 1);
  cfg->password[sizeof(cfg->password) - 1] = '\0';
  strncpy(cfg->api_key, key.c_str(), sizeof(cfg->api_key) - 1);
  cfg->api_key[sizeof(cfg->api_key) - 1] = '\0';
  cfg->refresh_interval = interval;
  strncpy(cfg->minimax_key, mmkey.c_str(), sizeof(cfg->minimax_key) - 1);
  cfg->minimax_key[sizeof(cfg->minimax_key) - 1] = '\0';
  cfg->provider_mode = mode > MODE_BOTH ? MODE_KIMI : mode;
  cfg->rotation = rot > 3 ? 0 : rot; // 脏数据兜底
  return cfg->ssid[0] != '\0' || cfg->api_key[0] != '\0';
}

bool config_store_save(const DeviceConfig* cfg) {
  if (!cfg) return false;
  Preferences p;
  if (!p.begin(NS, false)) return false;
  p.putString("ssid", cfg->ssid);
  p.putString("pass", cfg->password);
  p.putString("key", cfg->api_key);
  p.putLong("interval", cfg->refresh_interval);
  p.putString("mmkey", cfg->minimax_key);
  p.putUChar("mode", cfg->provider_mode);
  p.putUChar("rot", cfg->rotation);
  p.end();
  return true;
}

void config_store_clear() {
  Preferences p;
  if (p.begin(NS, false)) {
    p.clear();
    p.end();
  }
}

bool config_store_is_configured() {
  DeviceConfig cfg;
  if (!config_store_load(&cfg)) return false;
  return validate_config(&cfg) == CFG_OK;
}
