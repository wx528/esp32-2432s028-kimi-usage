#include "display.h"
#include <stdio.h>
#include <time.h>
#include <math.h>

static const uint16_t BG = TFT_BLACK;
static const int16_t RING_CX = 120;
static const int16_t RING_CY = 119;
static const int16_t RING_R = 75;
static const int16_t RING_W = 14;

static uint16_t level_color(UsageLevel lv) {
  switch (lv) {
    case LEVEL_WARNING:  return TFT_YELLOW;
    case LEVEL_CRITICAL: return TFT_RED;
    default:             return TFT_GREEN;
  }
}

void display_init(TFT_eSPI* tft) {
  tft->init();
  tft->setRotation(0);
  tft->fillScreen(BG);
  tft->setSwapBytes(true);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
}

static void draw_title(TFT_eSPI* tft) {
  tft->setTextColor(TFT_CYAN, BG);
  tft->setTextDatum(MC_DATUM);
  tft->drawString("KIMI USAGE", tft->width() / 2, 18, 4);
}

void display_draw_connecting(TFT_eSPI* tft) {
  tft->fillScreen(BG);
  draw_title(tft);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString("Connecting WiFi...", 120, 160, 2);
}

void display_draw_portal_hint(TFT_eSPI* tft, const char* ap_name, const char* ap_pass) {
  tft->fillScreen(BG);
  draw_title(tft);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString("Setup mode", 120, 60, 2);
  tft->setTextColor(TFT_YELLOW, BG);
  tft->drawString("Connect phone to WiFi:", 120, 110, 2);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString(ap_name, 120, 134, 4);
  tft->drawString(String("pass: ") + ap_pass, 120, 160, 2);
  tft->setTextColor(TFT_CYAN, BG);
  tft->drawString("then open 192.168.4.1", 120, 200, 2);
}

void display_draw_invalid_key(TFT_eSPI* tft) {
  tft->fillScreen(BG);
  draw_title(tft);
  tft->setTextColor(TFT_RED, BG);
  tft->drawString("INVALID API KEY", 120, 120, 4);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString("Hold BOOT 5s", 120, 170, 2);
  tft->drawString("to reconfigure", 120, 192, 2);
}

// fillArc 自由函数：TFT_eSPI 2.5.43 不带成员版，参考官方 TFT_FillArcSpiral 示例
// seg 单位 3 度（120 段 = 360°），start_seg=0 在正上方，顺时针
static void fill_arc(TFT_eSPI* tft, int x, int y, int r_outer, int r_inner, int start_seg, int seg_count, uint16_t colour) {
  const float SEG_DEG = 3.0f;
  for (int i = 0; i < seg_count; i++) {
    float a0 = (start_seg + i) * SEG_DEG;
    float a1 = (start_seg + i + 1) * SEG_DEG;
    float sx0 = cos((a0 - 90.0f) * DEG_TO_RAD);
    float sy0 = sin((a0 - 90.0f) * DEG_TO_RAD);
    float sx1 = cos((a1 - 90.0f) * DEG_TO_RAD);
    float sy1 = sin((a1 - 90.0f) * DEG_TO_RAD);
    int x0 = (int)(sx0 * r_inner) + x;
    int y0 = (int)(sy0 * r_inner) + y;
    int x1 = (int)(sx0 * r_outer) + x;
    int y1 = (int)(sy0 * r_outer) + y;
    int x2 = (int)(sx1 * r_inner) + x;
    int y2 = (int)(sy1 * r_inner) + y;
    int x3 = (int)(sx1 * r_outer) + x;
    int y3 = (int)(sy1 * r_outer) + y;
    tft->fillTriangle(x0, y0, x1, y1, x2, y2, colour);
    tft->fillTriangle(x1, y1, x2, y2, x3, y3, colour);
  }
}

// 画圆环：先整环画轨道色，再覆盖前景弧；只画到 percent 对应角度
static void draw_ring(TFT_eSPI* tft, int percent, UsageLevel lv) {
  uint16_t fg = level_color(lv);
  uint16_t track = TFT_DARKGREY;
  int segs_full = (percent * 120) / 100; // 120 段 = 360°
  fill_arc(tft, RING_CX, RING_CY, RING_R, RING_R - RING_W, 0, 120, track);
  if (segs_full > 0) {
    fill_arc(tft, RING_CX, RING_CY, RING_R, RING_R - RING_W, 0, segs_full, fg);
  }
}

void display_draw_main(TFT_eSPI* tft, const DisplayState& st) {
  tft->fillScreen(BG);
  draw_title(tft);

  if (st.key_invalid) { display_draw_invalid_key(tft); return; }

  if (st.has_data) {
    int pct = usage_percent(st.data.plan_used, st.data.plan_limit);
    UsageLevel lv = st.stale ? LEVEL_NORMAL : usage_level(pct);
    draw_ring(tft, pct, lv);

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
    tft->setTextColor(st.stale ? TFT_DARKGREY : level_color(lv), BG);
    tft->drawString(pctStr, RING_CX, RING_CY - 8, 4);
    tft->setTextColor(TFT_DARKGREY, BG);
    tft->drawString("WEEKLY", RING_CX, RING_CY + 16, 2);

    char usedBuf[16], limitBuf[16], line[40];
    format_thousands(st.data.plan_used, usedBuf, sizeof(usedBuf));
    format_thousands(st.data.plan_limit, limitBuf, sizeof(limitBuf));
    snprintf(line, sizeof(line), "%s / %s", usedBuf, limitBuf);
    tft->setTextColor(st.stale ? TFT_DARKGREY : TFT_WHITE, BG);
    tft->drawString(line, 120, 208, 2);

    char cd[32];
    if (st.clock_valid) {
      format_countdown((long)st.data.plan_reset - (long)time(nullptr), cd, sizeof(cd));
    } else {
      snprintf(cd, sizeof(cd), "resets --");
    }
    tft->setTextColor(TFT_DARKGREY, BG);
    tft->drawString(cd, 120, 230, 2);

    // 分隔线
    tft->drawFastHLine(12, 248, 216, TFT_DARKGREY);

    // 5H 窗口
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(TFT_YELLOW, BG);
    tft->drawString("5H WINDOW", 12, 258, 2);
    tft->setTextDatum(MC_DATUM);
    int wpct = usage_percent(st.data.window_used, st.data.window_limit);
    int barW = 216;
    int fillW = (wpct * barW) / 100;
    tft->drawRect(12, 276, barW, 12, TFT_DARKGREY);
    if (fillW > 0) tft->fillRect(12, 276, fillW, 12, st.stale ? TFT_DARKGREY : level_color(usage_level(wpct)));

    char wcd[32];
    if (st.clock_valid) {
      format_countdown((long)st.data.window_reset - (long)time(nullptr), wcd, sizeof(wcd));
    } else {
      snprintf(wcd, sizeof(wcd), "resets --");
    }
    char wline[48];
    snprintf(wline, sizeof(wline), "%ld/%ld  %s", st.data.window_used, st.data.window_limit, wcd);
    tft->setTextColor(TFT_DARKGREY, BG);
    tft->drawString(wline, 120, 296, 2);
  } else {
    tft->setTextColor(TFT_WHITE, BG);
    tft->drawString("Fetching...", 120, 160, 2);
  }

  // 状态栏
  char age[16] = "";
  if (st.has_data && st.age_seconds >= 60) format_age(st.age_seconds, age, sizeof(age));
  const char* wifi = st.wifi_ok ? "WiFi OK" : "WiFi LOST";
  const char* clk = st.clock_valid ? "" : " !";
  char status[64];
  if (st.status_msg && *st.status_msg) {
    snprintf(status, sizeof(status), "%s%s  %s", st.status_msg, clk, age);
  } else {
    snprintf(status, sizeof(status), "%s%s  %s", wifi, clk, age);
  }
  tft->setTextColor(st.wifi_ok ? TFT_DARKGREY : TFT_RED, BG);
  tft->setTextDatum(ML_DATUM);
  tft->drawString(status, 8, 310, 2);
}
