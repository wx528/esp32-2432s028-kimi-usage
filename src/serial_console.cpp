#include "serial_console.h"
#include "command_parser.h"
#include "config_validate.h"
#include "config_store.h"
#include "usage_types.h"
#include <string.h>

static SerialHooks s_hooks;
static String s_buf;
static const size_t MAX_LINE = 256;

void serial_console_begin(const SerialHooks& hooks) {
  s_hooks = hooks;
  s_buf = "";
}

static void print_config() {
  DeviceConfig c;
  config_store_load(&c);
  char masked[32], masked_mm[32];
  mask_api_key(c.api_key, masked, sizeof(masked));
  mask_api_key(c.minimax_key, masked_mm, sizeof(masked_mm));
  const char* mode = c.provider_mode == MODE_MINIMAX ? "minimax"
                   : c.provider_mode == MODE_BOTH ? "both" : "kimi";
  Serial.print("OK:CONFIG:{\"ssid\":\"");
  Serial.print(c.ssid);
  Serial.print("\",\"key\":\"");
  Serial.print(masked);
  Serial.print("\",\"mmkey\":\"");
  Serial.print(masked_mm);
  Serial.print("\",\"mode\":\"");
  Serial.print(mode);
  Serial.print("\",\"interval\":");
  Serial.print(c.refresh_interval);
  Serial.println("}");
}

static void execute(const Command& cmd) {
  switch (cmd.type) {
    case CMD_GET_CONFIG:
      print_config();
      break;
    case CMD_SET_WIFI: {
      DeviceConfig c;
      config_store_load(&c);
      strncpy(c.ssid, cmd.ssid, sizeof(c.ssid) - 1); c.ssid[sizeof(c.ssid)-1] = '\0';
      strncpy(c.password, cmd.password, sizeof(c.password) - 1); c.password[sizeof(c.password)-1] = '\0';
      config_store_save(&c);
      Serial.println("OK:SET:WIFI");
      if (s_hooks.on_config_changed) s_hooks.on_config_changed();
      break;
    }
    case CMD_SET_KEY: {
      DeviceConfig c;
      config_store_load(&c);
      strncpy(c.api_key, cmd.key, sizeof(c.api_key) - 1); c.api_key[sizeof(c.api_key)-1] = '\0';
      config_store_save(&c);
      Serial.println("OK:SET:KEY");
      if (s_hooks.on_config_changed) s_hooks.on_config_changed();
      break;
    }
    case CMD_SET_INTERVAL: {
      DeviceConfig c;
      config_store_load(&c);
      c.refresh_interval = cmd.interval;
      config_store_save(&c);
      Serial.println("OK:SET:INTERVAL");
      if (s_hooks.on_config_changed) s_hooks.on_config_changed();
      break;
    }
    case CMD_SET_PROVIDER: {
      DeviceConfig c;
      config_store_load(&c);
      c.provider_mode = cmd.provider_mode;
      config_store_save(&c);
      Serial.println("OK:SET:PROVIDER");
      if (s_hooks.on_config_changed) s_hooks.on_config_changed();
      break;
    }
    case CMD_SET_MMKEY: {
      DeviceConfig c;
      config_store_load(&c);
      strncpy(c.minimax_key, cmd.mmkey, sizeof(c.minimax_key) - 1); c.minimax_key[sizeof(c.minimax_key)-1] = '\0';
      config_store_save(&c);
      Serial.println("OK:SET:MMKEY");
      if (s_hooks.on_config_changed) s_hooks.on_config_changed();
      break;
    }
    case CMD_REFRESH:
      Serial.println("OK:REFRESH");
      if (s_hooks.on_refresh) s_hooks.on_refresh();
      break;
    case CMD_GET_USAGE:
      // 由 on_refresh 模式太重，这里简单提示；实际数据在屏幕上
      Serial.println("OK:USAGE:see display");
      break;
    case CMD_RESET_CONFIG:
      Serial.println("OK:RESET");
      if (s_hooks.on_reset_config) s_hooks.on_reset_config();
      break;
    case CMD_REBOOT:
      Serial.println("OK:REBOOT");
      delay(200);
      ESP.restart();
      break;
    default:
      Serial.println("ERR:UNKNOWN_CMD");
  }
}

void serial_console_poll() {
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      s_buf.trim();
      if (s_buf.length() > 0) {
        Command cmd;
        if (parse_command(s_buf.c_str(), &cmd)) {
          execute(cmd);
        } else {
          Serial.println("ERR:BAD_FORMAT");
        }
      }
      s_buf = "";
    } else {
      s_buf += ch;
      if (s_buf.length() > MAX_LINE) {
        s_buf = "";
        Serial.println("ERR:TOO_LONG");
      }
    }
  }
}
