#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <XPT2046_Touchscreen.h>
#include "app_state.h"
#include "config_validate.h"
#include "config_store.h"
#include "kimi_net.h"
#include "usage_parser.h"
#include "provider.h"
#include "display.h"
#include "portal.h"
#include "retry_policy.h"
#include "serial_console.h"

static const char* AP_NAME = "CYD-Kimi-Setup";
static const char* AP_PASS = "kimisetup";
static const uint32_t PORTAL_TIMEOUT_MS = 10UL * 60 * 1000;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20UL * 1000;
static const uint32_t NTP_WAIT_MS = 8UL * 1000;
static const int WIFI_FAIL_TO_PORTAL = 30;
static const uint8_t BOOT_PIN = 0;
static const uint32_t BOOT_HOLD_MS = 5000;

// CYD 触摸（XPT2046）走独立 HSPI
static const uint8_t TOUCH_CS = 33;
static const uint8_t TOUCH_IRQ = 36;
static const uint8_t TOUCH_SCK = 25;
static const uint8_t TOUCH_MISO = 39;
static const uint8_t TOUCH_MOSI = 32;
static const uint32_t TOUCH_DEBOUNCE_MS = 300;

static TFT_eSPI tft;
static AppState s_state = STATE_BOOT;
static DeviceConfig s_cfg;

struct ProviderSlot {
  bool has_data = false;
  UsageData data;
  uint32_t fetched_ms = 0;
  bool key_invalid = false;
  const char* status_msg = "";
  int api_fail_count = 0;
};
static ProviderSlot s_slots[2];          // 下标即 Provider 枚举值
static uint8_t s_active = PROVIDER_KIMI; // 当前显示/定时拉取的 provider
static uint8_t s_rotation = 0; // 显示方向 0-3
static SPIClass s_touch_spi(HSPI);
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);
static int s_wifi_fail_count = 0;
static uint32_t s_last_fetch_ms = 0;
static long s_next_interval_sec = 60;

static long data_age_seconds() {
  const ProviderSlot& s = s_slots[s_active];
  if (!s.has_data) return 0;
  return (long)((millis() - s.fetched_ms) / 1000UL);
}

static void redraw() {
  const ProviderSlot& s = s_slots[s_active];
  DisplayState st;
  memset(&st, 0, sizeof(st));
  st.has_data = s.has_data;
  st.data = s.data;
  st.stale = s.has_data && (data_age_seconds() > s_next_interval_sec + 15);
  st.age_seconds = data_age_seconds();
  st.clock_valid = net_time_valid();
  st.wifi_ok = (WiFi.status() == WL_CONNECTED);
  st.status_msg = s.status_msg;
  st.key_invalid = s.key_invalid;
  static char title[20];
  snprintf(title, sizeof(title), "%s USAGE", provider_name((Provider)s_active));
  st.title = title;
  st.switch_hint = (s_cfg.provider_mode == MODE_BOTH);
  display_draw_main(&tft, st);
}

static void enter_portal() {
  s_state = STATE_PORTAL;
  display_draw_portal_hint(&tft, AP_NAME, AP_PASS);
  PortalResult r = portal_run(AP_NAME, AP_PASS, PORTAL_TIMEOUT_MS); // 阻塞
  if (r.submitted) {
    config_store_save(&r.cfg);
  }
  ESP.restart(); // 提交成功或超时都重启
}

static void enter_connecting() {
  s_state = STATE_CONNECTING;
  display_draw_connecting(&tft);
  WiFi.mode(WIFI_STA);
  WiFi.begin(s_cfg.ssid, s_cfg.password);
}

// 拉取指定 provider 并写入对应槽。仅激活槽的结果会重绘屏幕。
static void fetch_provider(uint8_t idx) {
  ProviderSlot& s = s_slots[idx];
  if (idx == s_active) s_last_fetch_ms = millis();
  const char* key = idx == PROVIDER_MINIMAX ? s_cfg.minimax_key : s_cfg.api_key;
  NetResult r = net_fetch_usage((Provider)idx, key, 10000);
  if (r.status == NET_OK) {
    UsageData d;
    ParseResult pr = provider_parse((Provider)idx, r.body.c_str(), &d);
    if (pr == PARSE_OK) {
      s.data = d;
      s.has_data = true;
      s.fetched_ms = millis();
      s.key_invalid = false;
      s.status_msg = "";
      s.api_fail_count = 0;
      if (idx == s_active) s_next_interval_sec = s_cfg.refresh_interval;
    } else if (pr == PARSE_ERR_KEY_DISABLED) {
      s.key_invalid = true;
      s.status_msg = "";
    } else {
      s.status_msg = "BAD RESPONSE";
      s.api_fail_count++;
    }
  } else if (r.status == NET_ERR_HTTP && (r.http_code == 401 || r.http_code == 498)) {
    s.key_invalid = true;
    s.status_msg = "";
  } else if (r.status == NET_ERR_WIFI) {
    s.status_msg = "WiFi LOST";
  } else {
    s.status_msg = "API TIMEOUT";
    s.api_fail_count++;
    if (idx == s_active) s_next_interval_sec = retry_interval_sec(s_cfg.refresh_interval, s.api_fail_count);
  }
  if (idx == s_active) redraw();
}

static void fetch_and_update() { fetch_provider(s_active); }

static void hook_refresh() {
  if (s_state == STATE_RUNNING) fetch_and_update();
}
static void hook_config_changed() {
  config_store_load(&s_cfg);
  s_next_interval_sec = s_cfg.refresh_interval;
  for (int i = 0; i < 2; i++) {
    s_slots[i].key_invalid = false;
    s_slots[i].api_fail_count = 0;
  }
  // 单 provider 模式下激活项跟随配置
  if (s_cfg.provider_mode == MODE_KIMI) s_active = PROVIDER_KIMI;
  if (s_cfg.provider_mode == MODE_MINIMAX) s_active = PROVIDER_MINIMAX;
}
static void hook_reset_config() {
  config_store_clear();
  delay(200);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  delay(100);
  Serial.println("CYD Kimi Usage Ready");

  config_store_load(&s_cfg);          // 无配置时 s_cfg 保持零值（rotation=0）
  s_rotation = s_cfg.rotation;
  display_init(&tft, s_rotation);
  s_touch_spi.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  s_touch.begin(s_touch_spi);
  s_touch.setRotation(s_rotation);

  SerialHooks hooks{hook_refresh, hook_config_changed, hook_reset_config};
  serial_console_begin(hooks);

  if (!config_store_is_configured()) {
    enter_portal(); // 不返回（内部 restart）
    return;
  }
  s_active = s_cfg.provider_mode == MODE_MINIMAX ? PROVIDER_MINIMAX : PROVIDER_KIMI;
  s_next_interval_sec = s_cfg.refresh_interval;
  enter_connecting();
}

static void switch_provider() {
  s_active = s_active == PROVIDER_KIMI ? PROVIDER_MINIMAX : PROVIDER_KIMI;
  Serial.printf("OK:SWITCH:%s\n", provider_name((Provider)s_active));
  redraw();            // 有缓存先转灰显示，无缓存显示 Fetching...
  fetch_and_update();  // 立刻拉新激活的一家
}

static void check_touch_switch() {
  if (s_state != STATE_RUNNING) return;
  if (s_cfg.provider_mode != MODE_BOTH) return;
  static uint32_t last_tap = 0;
  static bool waiting_release = false;
  if (!s_touch.touched()) { waiting_release = false; return; }
  if (waiting_release) return;
  uint32_t now = millis();
  if (now - last_tap < TOUCH_DEBOUNCE_MS) return;
  last_tap = now;
  waiting_release = true;
  switch_provider();
}

static void cycle_rotation() {
  s_rotation = rotation_next(s_rotation);
  s_cfg.rotation = s_rotation;
  config_store_save(&s_cfg);
  Serial.printf("OK:ROTATION:%d\n", s_rotation * 90);
  display_rotate(&tft, s_rotation);
  s_touch.setRotation(s_rotation);
  redraw();
}

static void check_boot_long_press() {
  if (s_state != STATE_RUNNING && s_state != STATE_CONNECTING) return;
  static uint32_t press_start = 0;
  if (digitalRead(BOOT_PIN) == LOW) {
    if (press_start == 0) press_start = millis();
    uint32_t held = millis() - press_start;
    if (held >= BOOT_HOLD_MS) {
      Serial.println("OK:RESET (boot button)");
      config_store_clear();
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Config erased", tft.width() / 2, tft.height() / 2, 2);
      delay(1500);
      ESP.restart();
    } else if (held > 500) { // 按了 0.5 秒开始给倒数提示
      int remain = (int)((BOOT_HOLD_MS - held) / 1000) + 1;
      int16_t cw = tft.width(), ch = tft.height();
      tft.fillRect(0, ch - 40, cw, 20, TFT_BLACK);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      char msg[24];
      snprintf(msg, sizeof(msg), "Release to cancel %d", remain);
      tft.drawString(msg, cw / 2, ch - 30, 2);
    }
  } else {
    if (press_start != 0) {
      uint32_t held = millis() - press_start;
      press_start = 0;
      if (held < 500) {
        cycle_rotation(); // 短按：切换显示方向
      } else {
        redraw(); // 取消擦除倒数，恢复显示
      }
    }
  }
}

void loop() {
  serial_console_poll();
  check_boot_long_press();
  check_touch_switch();
  uint32_t now = millis();

  if (s_state == STATE_CONNECTING) {
    static uint32_t connect_start = 0;
    if (connect_start == 0) connect_start = now;
    if (WiFi.status() == WL_CONNECTED) {
      connect_start = 0;
      s_wifi_fail_count = 0;
      net_time_begin();
      net_time_wait(NTP_WAIT_MS); // 失败不阻塞，fetch 内部会降级
      s_state = STATE_RUNNING;
      fetch_and_update();
    } else if (now - connect_start > WIFI_CONNECT_TIMEOUT_MS) {
      connect_start = 0;
      enter_portal(); // 连不上 → 重新配网
      return;
    }
    delay(100);
    return;
  }

  if (s_state == STATE_RUNNING) {
    // WiFi 掉线统计
    if (WiFi.status() != WL_CONNECTED) {
      s_wifi_fail_count++;
      s_slots[s_active].status_msg = "WiFi LOST";
      WiFi.reconnect();
      if (s_wifi_fail_count >= WIFI_FAIL_TO_PORTAL) {
        enter_portal();
        return;
      }
      redraw();
      delay(10000); // 每 10 秒重连
      return;
    }
    s_wifi_fail_count = 0;

    // 到点拉取
    if (!s_slots[s_active].key_invalid && now - s_last_fetch_ms >= (uint32_t)s_next_interval_sec * 1000UL) {
      fetch_and_update();
    }

    // 定期刷新屏幕上的"数据年龄"
    static uint32_t last_age_redraw = 0;
    if (now - last_age_redraw >= 30000UL) {
      last_age_redraw = now;
      redraw();
    }
  }
  delay(50);
}
