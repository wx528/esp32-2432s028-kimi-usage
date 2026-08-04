#pragma once
#include <TFT_eSPI.h>
#include "usage_types.h"
#include "display_model.h"
#include "format_utils.h"

struct DisplayState {
  bool has_data;        // 是否拿到过数据
  UsageData data;       // has_data 时的数据
  bool stale;           // 数据是否过期（非本周期新取）
  long age_seconds;     // 数据年龄
  bool clock_valid;     // 时钟有效（决定倒计时 + 证书校验状态）
  bool wifi_ok;
  const char* status_msg; // 状态栏文字，如 "WiFi LOST"/"API TIMEOUT"/"BAD RESPONSE"
  bool key_invalid;     // 401/498 → 全屏错误页
};

void display_init(TFT_eSPI* tft);
void display_draw_portal_hint(TFT_eSPI* tft, const char* ap_name, const char* ap_pass);
void display_draw_connecting(TFT_eSPI* tft);
void display_draw_invalid_key(TFT_eSPI* tft);
void display_draw_main(TFT_eSPI* tft, const DisplayState& st);
