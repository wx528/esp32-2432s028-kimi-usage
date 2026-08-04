#include "command_parser.h"
#include <string.h>
#include <stdlib.h>

static void copy_str(char* dst, int dst_size, const char* src) {
  int i = 0;
  while (src[i] && i < dst_size - 1) { dst[i] = src[i]; i++; }
  dst[i] = '\0';
}

bool parse_command(const char* line, Command* out) {
  if (!line || !out || !*line) return false;
  out->type = CMD_UNKNOWN;
  out->ssid[0] = out->password[0] = out->key[0] = '\0';
  out->interval = 0;

  if (strcmp(line, "GET:CONFIG") == 0)  { out->type = CMD_GET_CONFIG; return true; }
  if (strcmp(line, "GET:USAGE") == 0)   { out->type = CMD_GET_USAGE; return true; }
  if (strcmp(line, "REFRESH") == 0)     { out->type = CMD_REFRESH; return true; }
  if (strcmp(line, "RESET:CONFIG") == 0){ out->type = CMD_RESET_CONFIG; return true; }
  if (strcmp(line, "REBOOT") == 0)      { out->type = CMD_REBOOT; return true; }

  if (strncmp(line, "SET:KEY:", 8) == 0) {
    const char* key = line + 8;
    if (!*key) return false;
    copy_str(out->key, sizeof(out->key), key);
    out->type = CMD_SET_KEY;
    return true;
  }

  if (strncmp(line, "SET:INTERVAL:", 13) == 0) {
    const char* num = line + 13;
    if (!*num) return false;
    char* end = nullptr;
    long v = strtol(num, &end, 10);
    if (end == num || *end != '\0') return false; // 非纯数字
    if (v < 30 || v > 3600) return false;
    out->interval = v;
    out->type = CMD_SET_INTERVAL;
    return true;
  }

  if (strncmp(line, "SET:WIFI:", 9) == 0) {
    const char* rest = line + 9;
    const char* colon = strchr(rest, ':');
    if (!colon) return false;                 // 至少要分出 ssid 和 password 两段
    if (colon == rest) return false;          // 空 ssid
    int ssid_len = (int)(colon - rest);
    if (ssid_len >= (int)sizeof(out->ssid)) return false;
    memcpy(out->ssid, rest, ssid_len);
    out->ssid[ssid_len] = '\0';
    copy_str(out->password, sizeof(out->password), colon + 1); // 密码取剩余全部，可含冒号
    out->type = CMD_SET_WIFI;
    return true;
  }

  return false;
}
