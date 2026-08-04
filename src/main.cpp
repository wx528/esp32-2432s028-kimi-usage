#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "app_state.h"
#include "config_validate.h"
#include "config_store.h"
#include "kimi_net.h"
#include "usage_parser.h"
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

static TFT_eSPI tft;
static AppState s_state = STATE_BOOT;
static DeviceConfig s_cfg;

static bool s_has_data = false;
static UsageData s_data;
static uint32_t s_data_fetched_ms = 0; // millis()，用于算数据年龄
static bool s_key_invalid = false;
static const char* s_status_msg = "";
static int s_wifi_fail_count = 0;
static int s_api_fail_count = 0;
static uint32_t s_last_fetch_ms = 0;
static long s_next_interval_sec = 60;

static long data_age_seconds() {
  if (!s_has_data) return 0;
  return (long)((millis() - s_data_fetched_ms) / 1000UL);
}

static void redraw() {
  DisplayState st;
  st.has_data = s_has_data;
  st.data = s_data;
  st.stale = s_has_data && (data_age_seconds() > s_next_interval_sec + 15);
  st.age_seconds = data_age_seconds();
  st.clock_valid = net_time_valid();
  st.wifi_ok = (WiFi.status() == WL_CONNECTED);
  st.status_msg = s_status_msg;
  st.key_invalid = s_key_invalid;
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

static void fetch_and_update() {
  s_last_fetch_ms = millis();
  NetResult r = kimi_fetch_usage(s_cfg.api_key, 10000);
  if (r.status == NET_OK) {
    UsageData d;
    ParseResult pr = parse_usage_json(r.body.c_str(), &d);
    if (pr == PARSE_OK) {
      s_data = d;
      s_has_data = true;
      s_data_fetched_ms = millis();
      s_key_invalid = false;
      s_status_msg = "";
      s_api_fail_count = 0;
      s_next_interval_sec = s_cfg.refresh_interval;
    } else if (pr == PARSE_ERR_KEY_DISABLED) {
      s_key_invalid = true;
      s_status_msg = "";
    } else {
      s_status_msg = "BAD RESPONSE";
      s_api_fail_count++;
    }
  } else if (r.status == NET_ERR_HTTP && (r.http_code == 401 || r.http_code == 498)) {
    s_key_invalid = true;
    s_status_msg = "";
  } else if (r.status == NET_ERR_WIFI) {
    s_status_msg = "WiFi LOST";
  } else {
    s_status_msg = "API TIMEOUT";
    s_api_fail_count++;
    s_next_interval_sec = retry_interval_sec(s_cfg.refresh_interval, s_api_fail_count);
  }
  redraw();
}

static void hook_refresh() {
  if (s_state == STATE_RUNNING) fetch_and_update();
}
static void hook_config_changed() {
  config_store_load(&s_cfg);
  s_next_interval_sec = s_cfg.refresh_interval;
  s_key_invalid = false;
  s_api_fail_count = 0;
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

  display_init(&tft);

  SerialHooks hooks{hook_refresh, hook_config_changed, hook_reset_config};
  serial_console_begin(hooks);

  if (!config_store_is_configured()) {
    enter_portal(); // 不返回（内部 restart）
    return;
  }
  config_store_load(&s_cfg);
  s_next_interval_sec = s_cfg.refresh_interval;
  enter_connecting();
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
      tft.drawString("Config erased", 120, 150, 2);
      delay(1500);
      ESP.restart();
    } else if (held > 500) { // 按了 0.5 秒开始给倒数提示
      int remain = (int)((BOOT_HOLD_MS - held) / 1000) + 1;
      tft.fillRect(0, 280, 240, 20, TFT_BLACK);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      char msg[24];
      snprintf(msg, sizeof(msg), "Release to cancel %d", remain);
      tft.drawString(msg, 120, 290, 2);
    }
  } else {
    if (press_start != 0) {
      press_start = 0;
      redraw(); // 松开恢复显示
    }
  }
}

void loop() {
  serial_console_poll();
  check_boot_long_press();
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
      s_status_msg = "WiFi LOST";
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
    if (!s_key_invalid && now - s_last_fetch_ms >= (uint32_t)s_next_interval_sec * 1000UL) {
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
