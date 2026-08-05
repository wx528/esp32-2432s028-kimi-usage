#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>

static const uint16_t BG = TFT_BLACK;
static const char* STATIC_TITLE = "USAGE MONITOR";

static uint8_t s_rotation = 0;
static bool is_landscape() { return s_rotation == 1 || s_rotation == 3; }

// ---- 双布局表 ----
struct Layout {
  int16_t title_y;
  int16_t ring_cx, ring_cy, ring_r, ring_w;
  int16_t num_cx, num_y;      // 周数字
  int16_t cd_cx, cd_y;        // 周倒计时
  int16_t div_x, div_y, div_w; // 分隔线
  int16_t win_lbl_x, win_lbl_y; // 5H WINDOW
  int16_t bar_x, bar_y, bar_w, bar_h;
  int16_t wline_cx, wline_y;  // 窗口行
  int16_t status_y;
};

// 竖屏 240×320（与原布局一致）
static const Layout LAY_P = {
  18,
  120, 119, 75, 14,
  120, 208,
  120, 230,
  12, 248, 216,
  12, 258,
  12, 276, 216, 12,
  120, 296,
  310
};

// 横屏 320×240（左环右栏）
static const Layout LAY_L = {
  14,
  78, 130, 60, 12,
  225, 95,
  225, 120,
  150, 140, 150,
  150, 152,
  150, 168, 150, 12,
  225, 192,
  226
};

static const Layout& layout() { return is_landscape() ? LAY_L : LAY_P; }

static uint16_t level_color(UsageLevel lv) {
  switch (lv) {
    case LEVEL_WARNING:  return TFT_YELLOW;
    case LEVEL_CRITICAL: return TFT_RED;
    default:             return TFT_GREEN;
  }
}

void display_init(TFT_eSPI* tft, uint8_t rotation) {
  s_rotation = rotation & 3;
  tft->init();
  tft->setRotation(s_rotation);
  tft->fillScreen(BG);
  tft->setSwapBytes(true);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
}

void display_rotate(TFT_eSPI* tft, uint8_t rotation) {
  s_rotation = rotation & 3;
  tft->setRotation(s_rotation);
}

static void draw_title(TFT_eSPI* tft, const char* title) {
  tft->setTextColor(TFT_CYAN, BG);
  tft->setTextDatum(MC_DATUM);
  tft->drawString(title, tft->width() / 2, layout().title_y, 4);
}

// 静态页：按 width()/height() 相对居中，四方向通用
void display_draw_connecting(TFT_eSPI* tft) {
  tft->fillScreen(BG);
  draw_title(tft, STATIC_TITLE);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString("Connecting WiFi...", tft->width() / 2, tft->height() / 2, 2);
}

void display_draw_portal_hint(TFT_eSPI* tft, const char* ap_name, const char* ap_pass) {
  tft->fillScreen(BG);
  draw_title(tft, STATIC_TITLE);
  int16_t cx = tft->width() / 2, h = tft->height();
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString("Setup mode", cx, h * 3 / 16, 2);
  tft->setTextColor(TFT_YELLOW, BG);
  tft->drawString("Connect phone to WiFi:", cx, h * 11 / 32, 2);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString(ap_name, cx, h * 7 / 16, 4);
  tft->drawString(String("pass: ") + ap_pass, cx, h / 2, 2);
  tft->setTextColor(TFT_CYAN, BG);
  tft->drawString("then open 192.168.4.1", cx, h * 5 / 8, 2);
}

void display_draw_invalid_key(TFT_eSPI* tft) {
  tft->fillScreen(BG);
  draw_title(tft, STATIC_TITLE);
  int16_t cx = tft->width() / 2, cy = tft->height() / 2;
  tft->setTextColor(TFT_RED, BG);
  tft->drawString("INVALID API KEY", cx, cy - 20, 4);
  tft->setTextColor(TFT_WHITE, BG);
  tft->drawString("Hold BOOT 5s", cx, cy + 22, 2);
  tft->drawString("to reconfigure", cx, cy + 44, 2);
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
static void draw_ring(TFT_eSPI* tft, const Layout& L, int percent, UsageLevel lv) {
  uint16_t fg = level_color(lv);
  uint16_t track = TFT_DARKGREY;
  int segs_full = (percent * 120) / 100; // 120 段 = 360°
  fill_arc(tft, L.ring_cx, L.ring_cy, L.ring_r, L.ring_r - L.ring_w, 0, 120, track);
  if (segs_full > 0) {
    fill_arc(tft, L.ring_cx, L.ring_cy, L.ring_r, L.ring_r - L.ring_w, 0, segs_full, fg);
  }
}

void display_draw_main(TFT_eSPI* tft, const DisplayState& st) {
  const Layout& L = layout();
  tft->fillScreen(BG);
  draw_title(tft, st.title ? st.title : STATIC_TITLE);

  if (st.key_invalid) { display_draw_invalid_key(tft); return; }

  if (st.has_data) {
    int pct = usage_percent(st.data.plan_used, st.data.plan_limit);
    UsageLevel lv = st.stale ? LEVEL_NORMAL : usage_level(pct);
    draw_ring(tft, L, pct, lv);

    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
    tft->setTextColor(st.stale ? TFT_DARKGREY : level_color(lv), BG);
    tft->drawString(pctStr, L.ring_cx, L.ring_cy - 8, 4);
    tft->setTextColor(TFT_DARKGREY, BG);
    tft->drawString("WEEKLY", L.ring_cx, L.ring_cy + 16, 2);

    char usedBuf[16], limitBuf[16], line[40];
    format_thousands(st.data.plan_used, usedBuf, sizeof(usedBuf));
    format_thousands(st.data.plan_limit, limitBuf, sizeof(limitBuf));
    snprintf(line, sizeof(line), "%s / %s", usedBuf, limitBuf);
    tft->setTextColor(st.stale ? TFT_DARKGREY : TFT_WHITE, BG);
    tft->drawString(line, L.num_cx, L.num_y, 2);

    char cd[32];
    if (st.clock_valid) {
      format_countdown((long)st.data.plan_reset - (long)time(nullptr), cd, sizeof(cd));
    } else {
      snprintf(cd, sizeof(cd), "resets --");
    }
    tft->setTextColor(TFT_DARKGREY, BG);
    tft->drawString(cd, L.cd_cx, L.cd_y, 2);

    // 分隔线
    tft->drawFastHLine(L.div_x, L.div_y, L.div_w, TFT_DARKGREY);

    // 5H 窗口
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(TFT_YELLOW, BG);
    tft->drawString("5H WINDOW", L.win_lbl_x, L.win_lbl_y, 2);
    tft->setTextDatum(MC_DATUM);
    int wpct = usage_percent(st.data.window_used, st.data.window_limit);
    int fillW = (wpct * L.bar_w) / 100;
    tft->drawRect(L.bar_x, L.bar_y, L.bar_w, L.bar_h, TFT_DARKGREY);
    if (fillW > 0) tft->fillRect(L.bar_x, L.bar_y, fillW, L.bar_h, st.stale ? TFT_DARKGREY : level_color(usage_level(wpct)));

    char wcd[32];
    if (st.clock_valid) {
      format_countdown((long)st.data.window_reset - (long)time(nullptr), wcd, sizeof(wcd));
    } else {
      snprintf(wcd, sizeof(wcd), "resets --");
    }
    char wline[48];
    snprintf(wline, sizeof(wline), "%ld/%ld  %s", st.data.window_used, st.data.window_limit, wcd);
    tft->setTextColor(TFT_DARKGREY, BG);
    tft->drawString(wline, L.wline_cx, L.wline_y, 2);
  } else {
    tft->setTextColor(TFT_WHITE, BG);
    tft->drawString("Fetching...", tft->width() / 2, tft->height() / 2, 2);
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
  if (st.switch_hint) {
    size_t len = strlen(status);
    snprintf(status + len, sizeof(status) - len, "%s", len > 0 && status[len-1] != ' ' ? "  tap: switch" : "tap: switch");
  }
  tft->setTextColor(st.wifi_ok ? TFT_DARKGREY : TFT_RED, BG);
  tft->setTextDatum(ML_DATUM);
  tft->drawString(status, 8, L.status_y, 2);
}
