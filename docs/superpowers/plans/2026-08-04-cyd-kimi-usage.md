# ESP32-2432S028 Kimi 用量显示器 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 ESP32-2432S028 独立显示 Kimi Coding Plan 用量：设备连 WiFi 直连 Kimi API，手机配网页面录入凭据，配置持久化在 NVS。

**Architecture:** 纯逻辑与硬件 I/O 分离。纯逻辑（JSON 解析、倒计时/数字格式化、配色判定、命令解析、配置校验、退避计算）放在 `lib/core/`，可在 PC 上用 `pio test -e native` 单测；硬件层（TFT、NVS、HTTPS、WebServer、串口）在 `src/` 只做薄封装，靠手工清单验证。状态机四个状态：BOOT → PORTAL / CONNECTING → RUNNING。

**Tech Stack:** PlatformIO + Arduino framework (esp32dev) · TFT_eSPI 2.5.43 · ArduinoJson 7 · ESP32 core 自带 WebServer/DNSServer/Preferences/WiFiClientSecure · Python 环境用 uv 管理（platformio + pyserial）· native 单测用 MinGW（scoop 安装）

**Spec:** `docs/superpowers/specs/2026-08-04-cyd-kimi-usage-design.md`

**项目根目录:** 本仓库根目录（下称 `$root`）。git 仓库已初始化，已有 `.gitignore`（含 `.pio/`、`.venv/`、`.superpowers/`）和已提交的设计文档。

## 文件结构

```
esp32-2432s028-kimi-usage/
├── .gitignore                     已有
├── pyproject.toml                 Task 0（uv 管理）
├── platformio.ini                 Task 0
├── AGENTS.md                      Task 15（烧录流程 + 硬件验证清单）
├── scripts/
│   ├── fix_spawn_cwd.py           Task 0（PlatformIO extra_script，Windows 修复）
│   └── send_command.py            Task 13（串口后门交互脚本）
├── lib/core/src/
│   ├── usage_types.h              Task 1：UsageData / ParseResult
│   ├── time_parse.{h,cpp}         Task 1：ISO8601 → epoch（纯）
│   ├── usage_parser.{h,cpp}       Task 2：JSON → UsageData（纯，ArduinoJson）
│   ├── format_utils.{h,cpp}       Task 3：千位分隔 / 倒计时 / 数据年龄（纯）
│   ├── display_model.{h,cpp}      Task 4：usage_percent / 配色等级（纯）
│   ├── command_parser.{h,cpp}     Task 5：串口命令解析（纯）
│   ├── config_validate.{h,cpp}    Task 6：配置校验 + key 遮蔽（纯）
│   ├── retry_policy.{h,cpp}       Task 7：指数退避（纯）
│   └── library.json               Task 1
└── src/
    ├── config_store.{h,cpp}       Task 8：NVS 读写（Preferences）
    ├── root_ca.h                  Task 9：DigiCert Global Root G2 PEM
    ├── kimi_net.{h,cpp}           Task 9：HTTPS fetch + NTP 对时
    ├── display.{h,cpp}            Task 10：全部绘屏
    ├── portal.{h,cpp}             Task 11：AP + DNS 劫持 + 中文表单
    ├── serial_console.{h,cpp}     Task 13：串口后门
    ├── app_state.h                Task 12：AppState 枚举
    └── main.cpp                   Task 0 骨架；Task 12 状态机；Task 14 BOOT 长按
```

约定：

- 纯逻辑一律用 `const char*` / `char buf[]` / 整型，不用 Arduino `String`，保证 native 可编译。
- `display.cpp` 内部把 core 的 `UsageLevel` 映射成 `TFT_GREEN/TFT_YELLOW/TFT_RED`。
- 所有屏幕文案英文；只有配网网页是中文。

---

### Task 0: 脚手架（uv、platformio.ini、native 工具链、最小 main）

**Files:**
- Create: `pyproject.toml`（uv 生成后追加依赖）
- Create: `platformio.ini`
- Create: `scripts/fix_spawn_cwd.py`
- Create: `src/main.cpp`（骨架）

- [ ] **Step 1: 用 uv 初始化 Python 项目并加入依赖**

```powershell
cd $root
uv init --bare --python 3.11
uv add platformio pyserial
```

Expected: 生成 `pyproject.toml`、`uv.lock`、`.venv/`。此后 pio 一律用 `.venv\Scripts\pio.exe`（`uv run pio` 等价）。

- [ ] **Step 2: 安装 native 单测所需的 C++ 编译器（MinGW）**

```powershell
scoop install mingw-winlibs
```

Expected: `g++ --version` 有输出（若提示重启 shell，重开一个）。这是 `platform = native` 的前提。

- [ ] **Step 3: 写 `platformio.ini`（esp32 + native 两个环境）**

```ini
[env:esp32-2432s028]
platform = espressif32@6.10.0
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
upload_port = COM5
monitor_port = COM5

lib_deps =
    bodmer/TFT_eSPI @ ^2.5.43
    bblanchon/ArduinoJson @ ^7.3.0

extra_scripts = pre:scripts/fix_spawn_cwd.py

build_flags =
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12
    -DTFT_MOSI=13
    -DTFT_SCLK=14
    -DTFT_CS=15
    -DTFT_DC=2
    -DTFT_RST=4
    -DTFT_BL=21
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_FONT4=1
    -DLOAD_FONT6=1
    -DLOAD_FONT7=1
    -DLOAD_FONT8=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=20000000

[env:native]
platform = native
test_build_src = no
build_flags =
    -std=gnu++17
    -DUNIT_TEST
lib_deps =
    bblanchon/ArduinoJson @ ^7.3.0
```

`test_build_src = no` 是关键：native 环境不编译 `src/` 里的 Arduino 代码，只编译 `lib/core`（测试 `#include` 到的部分）+ `test/`。

- [ ] **Step 4: 写 `scripts/fix_spawn_cwd.py`（从参考项目原样复制）**

```python
# 修复 PlatformIO/SCons 在 Windows 上调用 cmd.exe 时当前工作目录丢失的问题。
# 在每条命令前显式加上 "cd /d <项目目录> &&"，确保相对路径能正确解析。
import subprocess

Import("env")

_project_dir = env.subst("$PROJECT_DIR")

def _spawn_with_cwd(sh, escape, cmd, args, env):
    # args[0] 通常是程序名（已被 SCons 转义过），直接用空格拼接成命令行
    command_line = " ".join(args)
    wrapped = f'cd /d "{_project_dir}" && {command_line}'
    return subprocess.call(wrapped, shell=True, env=env)

env['SPAWN'] = _spawn_with_cwd
```

- [ ] **Step 5: 写最小 `src/main.cpp`（先证明能编过、能点亮屏）**

```cpp
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("CYD Kimi Usage scaffold ready");

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("KIMI USAGE", tft.width() / 2, 18, 4);
}

void loop() {
  delay(100);
}
```

- [ ] **Step 6: 验证 esp32 环境编译通过**

Run: `cd $root; .venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`（首次会拉 toolchain/TFT_eSPI/ArduinoJson，耗时几分钟正常）

- [ ] **Step 7: Commit**

```bash
git add pyproject.toml uv.lock platformio.ini scripts src/main.cpp
git commit -m "chore: scaffold PlatformIO project with uv-managed env"
```

---

### Task 1: 时间解析 time_parse（ISO8601 → epoch）

**Files:**
- Create: `lib/core/library.json`
- Create: `lib/core/src/usage_types.h`
- Create: `lib/core/src/time_parse.h`
- Create: `lib/core/src/time_parse.cpp`
- Test: `test/test_time_parse/main.cpp`

- [ ] **Step 1: 写 `lib/core/library.json`**

```json
{
  "name": "core",
  "version": "1.0.0",
  "description": "Pure logic for CYD Kimi usage display",
  "frameworks": "*",
  "platforms": "*"
}
```

- [ ] **Step 2: 写 `lib/core/src/usage_types.h`（贯穿全项目的数据类型）**

```cpp
#pragma once
#include <stdint.h>

// API 取回、解析后的结构化数据。reset 均为 UTC epoch 秒。
struct UsageData {
  long plan_used;      // 周额度已用
  long plan_limit;     // 周额度上限
  uint32_t plan_reset; // 周额度重置时刻
  long window_used;    // 5 小时窗口已用
  long window_limit;   // 5 小时窗口上限
  uint32_t window_reset;
};

enum ParseResult : uint8_t {
  PARSE_OK = 0,
  PARSE_ERR_JSON,       // 不是合法 JSON
  PARSE_ERR_MISSING,    // 缺少必要字段
  PARSE_ERR_BAD_VALUE,  // 字段值类型/格式不对
  PARSE_ERR_KEY_DISABLED // boosterWallet.status == STATUS_DISABLED
};
```

- [ ] **Step 3: 写失败测试 `test/test_time_parse/main.cpp`**

```cpp
#include <unity.h>
#include "time_parse.h"

void test_full_iso8601_with_fraction_and_z() {
  TEST_ASSERT_TRUE(parse_iso8601_epoch("2026-07-21T06:38:42.676140Z") != 0);
  // 与已知 epoch 对照：2026-07-21T06:38:42Z
  TEST_ASSERT_EQUAL_UINT32(1784615922UL, parse_iso8601_epoch("2026-07-21T06:38:42.676140Z"));
}

void test_iso8601_without_fraction() {
  TEST_ASSERT_EQUAL_UINT32(1784615922UL, parse_iso8601_epoch("2026-07-21T06:38:42Z"));
}

void test_epoch_boundary() {
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch("1970-01-01T00:00:00Z"));
}

void test_garbage_returns_zero() {
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch(""));
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch("not a date"));
  TEST_ASSERT_EQUAL_UINT32(0UL, parse_iso8601_epoch("2026-07-21"));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_full_iso8601_with_fraction_and_z);
  RUN_TEST(test_iso8601_without_fraction);
  RUN_TEST(test_epoch_boundary);
  RUN_TEST(test_garbage_returns_zero);
  return UNITY_END();
}
```

- [ ] **Step 4: 跑测试确认失败**

Run: `cd $root; .venv\Scripts\pio.exe test -e native -f test_time_parse`
Expected: FAIL，编译错误 `time_parse.h: No such file or directory`

- [ ] **Step 5: 写 `lib/core/src/time_parse.h`**

```cpp
#pragma once
#include <stdint.h>

// 解析形如 "2026-07-21T06:38:42.676140Z" 的 ISO8601（UTC）为 epoch 秒。
// 解析失败返回 0。不含时区转换，输入一律按 UTC 处理。
uint32_t parse_iso8601_epoch(const char* s);
```

- [ ] **Step 6: 写 `lib/core/src/time_parse.cpp`（days-from-civil 算法，无 timegm 依赖）**

```cpp
#include "time_parse.h"
#include <string.h>
#include <stdlib.h>

// Howard Hinnant 的 days_from_civil：公历年月日 → 自 1970-01-01 的天数
static long days_from_civil(long y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);            // [0, 399]
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;  // [0, 146096]
  return era * 146097L + (long)doe - 719468L;
}

static int take_digits(const char*& p, int n) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (*p < '0' || *p > '9') return -1;
    v = v * 10 + (*p - '0');
    p++;
  }
  return v;
}

uint32_t parse_iso8601_epoch(const char* s) {
  if (!s || strlen(s) < 20) return 0;
  const char* p = s;
  int y = take_digits(p, 4); if (y < 0 || *p != '-') return 0; p++;
  int mo = take_digits(p, 2); if (mo < 1 || mo > 12 || *p != '-') return 0; p++;
  int d = take_digits(p, 2); if (d < 1 || d > 31 || *p != 'T') return 0; p++;
  int h = take_digits(p, 2); if (h < 0 || h > 23 || *p != ':') return 0; p++;
  int mi = take_digits(p, 2); if (mi < 0 || mi > 59 || *p != ':') return 0; p++;
  int sec = take_digits(p, 2); if (sec < 0 || sec > 59) return 0;

  long days = days_from_civil(y, (unsigned)mo, (unsigned)d);
  long secs = days * 86400L + (long)h * 3600L + (long)mi * 60L + sec;
  if (secs < 0) return 0;
  return (uint32_t)secs;
}
```

- [ ] **Step 7: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_time_parse`
Expected: PASS

- [ ] **Step 8: Commit**

```bash
git add lib/core test/test_time_parse
git commit -m "feat(core): add ISO8601 to epoch parser"
```

---

### Task 2: 用量解析 usage_parser（JSON → UsageData）

**Files:**
- Create: `lib/core/src/usage_parser.h`
- Create: `lib/core/src/usage_parser.cpp`
- Test: `test/test_usage_parser/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_usage_parser/main.cpp`（用例覆盖桌面端工具的同款数据）**

```cpp
#include <unity.h>
#include <string.h>
#include "usage_parser.h"

static const char* SAMPLE = R"json({
  "usage": {"limit":"100","used":"89","remaining":"11","resetTime":"2026-07-21T06:38:42.676140Z"},
  "limits":[
    {"window":{"duration":300,"timeUnit":"TIME_UNIT_MINUTE"},
     "detail":{"limit":"100","used":"7","remaining":"93","resetTime":"2026-07-17T20:38:42.676140Z"}}
  ]
})json";

void test_parse_extracts_all_fields() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_usage_json(SAMPLE, &d));
  TEST_ASSERT_EQUAL_LONG(89, d.plan_used);
  TEST_ASSERT_EQUAL_LONG(100, d.plan_limit);
  TEST_ASSERT_EQUAL_UINT32(1784615922UL, d.plan_reset);
  TEST_ASSERT_EQUAL_LONG(7, d.window_used);
  TEST_ASSERT_EQUAL_LONG(100, d.window_limit);
  TEST_ASSERT_EQUAL_UINT32(1784320722UL, d.window_reset);
}

void test_computes_used_from_remaining_when_used_absent() {
  const char* j = R"json({
    "usage":{"limit":"100","remaining":"11","resetTime":"2026-07-21T06:38:42Z"},
    "limits":[{"detail":{"limit":"100","remaining":"93","resetTime":"2026-07-17T20:38:42Z"}}]
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_usage_json(j, &d));
  TEST_ASSERT_EQUAL_LONG(89, d.plan_used);
  TEST_ASSERT_EQUAL_LONG(7, d.window_used);
}

void test_missing_detail_is_missing_error() {
  const char* j = R"json({"usage":{"limit":"100","used":"1","resetTime":"2026-07-21T00:00:00Z"}})json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_MISSING, parse_usage_json(j, &d));
}

void test_invalid_json_is_json_error() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parse_usage_json("not json", &d));
}

void test_disabled_booster_wallet_is_key_disabled() {
  const char* j = R"json({
    "usage":{"limit":"100","remaining":"100","resetTime":"2026-07-28T06:38:42Z"},
    "limits":[{"detail":{"limit":"100","remaining":"100","resetTime":"2026-07-23T11:38:42Z"}}],
    "boosterWallet":{"status":"STATUS_DISABLED"}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_KEY_DISABLED, parse_usage_json(j, &d));
}

void test_enabled_booster_wallet_parses_fine() {
  const char* j = R"json({
    "usage":{"limit":"100","used":"10","resetTime":"2026-07-28T06:38:42Z"},
    "limits":[{"detail":{"limit":"100","used":"5","resetTime":"2026-07-23T11:38:42Z"}}],
    "boosterWallet":{"status":"STATUS_ENABLED"}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_usage_json(j, &d));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_parse_extracts_all_fields);
  RUN_TEST(test_computes_used_from_remaining_when_used_absent);
  RUN_TEST(test_missing_detail_is_missing_error);
  RUN_TEST(test_invalid_json_is_json_error);
  RUN_TEST(test_disabled_booster_wallet_is_key_disabled);
  RUN_TEST(test_enabled_booster_wallet_parses_fine);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `.venv\Scripts\pio.exe test -e native -f test_usage_parser`
Expected: FAIL，编译错误 `usage_parser.h: No such file or directory`

- [ ] **Step 3: 写 `lib/core/src/usage_parser.h`**

```cpp
#pragma once
#include "usage_types.h"

// 解析 Kimi /coding/v1/usages 响应体为 UsageData。
// 成功返回 PARSE_OK 并写入 *out；失败返回对应 ParseResult，out 不被修改。
ParseResult parse_usage_json(const char* json, UsageData* out);
```

- [ ] **Step 4: 写 `lib/core/src/usage_parser.cpp`**

```cpp
#include "usage_parser.h"
#include "time_parse.h"
#include <ArduinoJson.h>
#include <stdlib.h>

// 从 JsonVariant 取整数：优先 as<long>()；当值是字符串且非空时兜底 strtol
static bool json_to_long(JsonVariantConst v, long* out) {
  if (v.isNull()) return false;
  if (v.is<long>() || v.is<int>() || v.is<float>() || v.is<double>()) {
    *out = (long)v.as<double>();
    return true;
  }
  const char* s = v.as<const char*>();
  if (s && *s) {
    char* end = nullptr;
    long val = strtol(s, &end, 10);
    if (end != s) { *out = val; return true; }
  }
  return false;
}

ParseResult parse_usage_json(const char* json, UsageData* out) {
  if (!json || !out) return PARSE_ERR_MISSING;

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return PARSE_ERR_JSON;

  JsonVariantConst bw = doc["boosterWallet"];
  if (!bw.isNull()) {
    const char* st = bw["status"] | "";
    if (strcmp(st, "STATUS_DISABLED") == 0) return PARSE_ERR_KEY_DISABLED;
  }

  JsonVariantConst usage = doc["usage"];
  JsonVariantConst detail = doc["limits"][0]["detail"];
  if (usage.isNull() || detail.isNull()) return PARSE_ERR_MISSING;

  long plan_limit, plan_used, win_limit, win_used;
  if (!json_to_long(usage["limit"], &plan_limit)) return PARSE_ERR_BAD_VALUE;
  if (!json_to_long(detail["limit"], &win_limit)) return PARSE_ERR_BAD_VALUE;

  // used 优先取 "used"，缺失时用 limit - remaining
  long tmp;
  if (json_to_long(usage["used"], &tmp)) plan_used = tmp;
  else if (json_to_long(usage["remaining"], &tmp)) plan_used = plan_limit - tmp;
  else return PARSE_ERR_BAD_VALUE;

  if (json_to_long(detail["used"], &tmp)) win_used = tmp;
  else if (json_to_long(detail["remaining"], &tmp)) win_used = win_limit - tmp;
  else return PARSE_ERR_BAD_VALUE;

  const char* plan_reset_s = usage["resetTime"] | "";
  const char* win_reset_s = detail["resetTime"] | "";
  uint32_t plan_reset = parse_iso8601_epoch(plan_reset_s);
  uint32_t win_reset = parse_iso8601_epoch(win_reset_s);
  if (plan_reset == 0 || win_reset == 0) return PARSE_ERR_BAD_VALUE;

  out->plan_used = plan_used;
  out->plan_limit = plan_limit;
  out->plan_reset = plan_reset;
  out->window_used = win_used;
  out->window_limit = win_limit;
  out->window_reset = win_reset;
  return PARSE_OK;
}
```

（`strcmp` 需 `#include <string.h>`）

- [ ] **Step 5: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_usage_parser`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/usage_parser.* test/test_usage_parser
git commit -m "feat(core): add Kimi usage JSON parser"
```

---

### Task 3: 格式化 format_utils（千位分隔 / 倒计时 / 数据年龄）

**Files:**
- Create: `lib/core/src/format_utils.h`
- Create: `lib/core/src/format_utils.cpp`
- Test: `test/test_format_utils/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_format_utils/main.cpp`**

```cpp
#include <unity.h>
#include <string.h>
#include "format_utils.h"

void test_format_thousands() {
  char buf[24];
  TEST_ASSERT_EQUAL_STRING("0", format_thousands(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("60", format_thousands(60, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1,360", format_thousands(1360, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1,234,567", format_thousands(1234567, buf, sizeof(buf)));
}

void test_format_countdown_days_hours_minutes() {
  char buf[32];
  TEST_ASSERT_EQUAL_STRING("resets in 3d", format_countdown(3L*86400 + 3600, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resets in 5h", format_countdown(5L*3600 + 1800, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resets in 42m", format_countdown(42L*60 + 30, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resets in 1m", format_countdown(30, buf, sizeof(buf)));
}

void test_format_countdown_past_is_resetting() {
  char buf[32];
  TEST_ASSERT_EQUAL_STRING("resetting", format_countdown(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("resetting", format_countdown(-60, buf, sizeof(buf)));
}

void test_format_age() {
  char buf[24];
  TEST_ASSERT_EQUAL_STRING("just now", format_age(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("just now", format_age(59, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("5m ago", format_age(300, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL_STRING("1h ago", format_age(3600, buf, sizeof(buf)));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_format_thousands);
  RUN_TEST(test_format_countdown_days_hours_minutes);
  RUN_TEST(test_format_countdown_past_is_resetting);
  RUN_TEST(test_format_age);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `.venv\Scripts\pio.exe test -e native -f test_format_utils`
Expected: FAIL，编译错误 `format_utils.h: No such file or directory`

- [ ] **Step 3: 写 `lib/core/src/format_utils.h`**

```cpp
#pragma once

// 全部返回传入的 buf，便于直接当字符串用。所有函数保证不越界写 buf。
const char* format_thousands(long value, char* buf, int buf_size);
const char* format_countdown(long seconds, char* buf, int buf_size); // <0 视为已到期
const char* format_age(long seconds, char* buf, int buf_size);       // 数据年龄
```

- [ ] **Step 4: 写 `lib/core/src/format_utils.cpp`**

```cpp
#include "format_utils.h"
#include <stdio.h>
#include <stdlib.h>

const char* format_thousands(long value, char* buf, int buf_size) {
  char digits[24];
  snprintf(digits, sizeof(digits), "%ld", labs(value));
  int len = 0; while (digits[len]) len++;

  int out = 0;
  if (value < 0 && out < buf_size - 1) buf[out++] = '-';
  for (int i = 0; i < len && out < buf_size - 1; i++) {
    if (i > 0 && (len - i) % 3 == 0 && out < buf_size - 1) buf[out++] = ',';
    buf[out++] = digits[i];
  }
  buf[out] = '\0';
  return buf;
}

const char* format_countdown(long seconds, char* buf, int buf_size) {
  if (seconds <= 0) {
    snprintf(buf, buf_size, "resetting");
    return buf;
  }
  long minutes = seconds / 60;
  if (minutes < 60) {
    snprintf(buf, buf_size, "resets in %ldm", minutes < 1 ? 1 : minutes);
  } else if (minutes < 1440) {
    snprintf(buf, buf_size, "resets in %ldh", minutes / 60);
  } else {
    snprintf(buf, buf_size, "resets in %ldd", minutes / 1440);
  }
  return buf;
}

const char* format_age(long seconds, char* buf, int buf_size) {
  if (seconds < 60) {
    snprintf(buf, buf_size, "just now");
  } else if (seconds < 3600) {
    snprintf(buf, buf_size, "%ldm ago", seconds / 60);
  } else {
    snprintf(buf, buf_size, "%ldh ago", seconds / 3600);
  }
  return buf;
}
```

- [ ] **Step 5: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_format_utils`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/format_utils.* test/test_format_utils
git commit -m "feat(core): add number/countdown/age formatting"
```

---

### Task 4: 配色模型 display_model

**Files:**
- Create: `lib/core/src/display_model.h`
- Create: `lib/core/src/display_model.cpp`
- Test: `test/test_display_model/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_display_model/main.cpp`**

```cpp
#include <unity.h>
#include "display_model.h"

void test_usage_percent_clamps_0_100() {
  TEST_ASSERT_EQUAL_INT(0, usage_percent(0, 2000));
  TEST_ASSERT_EQUAL_INT(50, usage_percent(1000, 2000));
  TEST_ASSERT_EQUAL_INT(100, usage_percent(2000, 2000));
  TEST_ASSERT_EQUAL_INT(100, usage_percent(3000, 2000)); // 超出上限截断
  TEST_ASSERT_EQUAL_INT(0, usage_percent(-1, 2000));     // 负数截断
  TEST_ASSERT_EQUAL_INT(0, usage_percent(100, 0));       // 0 上限安全返回
}

void test_level_thresholds() {
  TEST_ASSERT_EQUAL(LEVEL_NORMAL,   usage_level(69));
  TEST_ASSERT_EQUAL(LEVEL_NORMAL,   usage_level(0));
  TEST_ASSERT_EQUAL(LEVEL_WARNING,  usage_level(70));
  TEST_ASSERT_EQUAL(LEVEL_WARNING,  usage_level(90));
  TEST_ASSERT_EQUAL(LEVEL_CRITICAL, usage_level(91));
  TEST_ASSERT_EQUAL(LEVEL_CRITICAL, usage_level(100));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_usage_percent_clamps_0_100);
  RUN_TEST(test_level_thresholds);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `.venv\Scripts\pio.exe test -e native -f test_display_model`
Expected: FAIL，编译错误 `display_model.h: No such file or directory`

- [ ] **Step 3: 写 `lib/core/src/display_model.h`**

```cpp
#pragma once
#include <stdint.h>

enum UsageLevel : uint8_t {
  LEVEL_NORMAL = 0,  // <70%
  LEVEL_WARNING,     // 70-90%
  LEVEL_CRITICAL     // >90%
};

// 已用百分比（0-100，越界截断；limit<=0 返回 0）
int usage_percent(long used, long limit);

// 百分比 → 配色等级。阈值：<70 绿，70-90 黄，>90 红
UsageLevel usage_level(int percent);
```

- [ ] **Step 4: 写 `lib/core/src/display_model.cpp`**

```cpp
#include "display_model.h"

int usage_percent(long used, long limit) {
  if (limit <= 0) return 0;
  if (used < 0) used = 0;
  if (used > limit) used = limit;
  return (int)((used * 100L) / limit);
}

UsageLevel usage_level(int percent) {
  if (percent < 70) return LEVEL_NORMAL;
  if (percent <= 90) return LEVEL_WARNING;
  return LEVEL_CRITICAL;
}
```

- [ ] **Step 5: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_display_model`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/display_model.* test/test_display_model
git commit -m "feat(core): add percent/color-level model"
```

---

### Task 5: 串口命令解析 command_parser

**Files:**
- Create: `lib/core/src/command_parser.h`
- Create: `lib/core/src/command_parser.cpp`
- Test: `test/test_command_parser/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_command_parser/main.cpp`**

```cpp
#include <unity.h>
#include <string.h>
#include "command_parser.h"

void test_no_arg_commands() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("GET:CONFIG", &c));
  TEST_ASSERT_EQUAL(CMD_GET_CONFIG, c.type);
  TEST_ASSERT_TRUE(parse_command("GET:USAGE", &c));
  TEST_ASSERT_EQUAL(CMD_GET_USAGE, c.type);
  TEST_ASSERT_TRUE(parse_command("REFRESH", &c));
  TEST_ASSERT_EQUAL(CMD_REFRESH, c.type);
  TEST_ASSERT_TRUE(parse_command("RESET:CONFIG", &c));
  TEST_ASSERT_EQUAL(CMD_RESET_CONFIG, c.type);
  TEST_ASSERT_TRUE(parse_command("REBOOT", &c));
  TEST_ASSERT_EQUAL(CMD_REBOOT, c.type);
}

void test_set_interval_valid_and_bounds() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:INTERVAL:60", &c));
  TEST_ASSERT_EQUAL(CMD_SET_INTERVAL, c.type);
  TEST_ASSERT_EQUAL_LONG(60, c.interval);

  TEST_ASSERT_FALSE(parse_command("SET:INTERVAL:29", &c));    // <30 非法
  TEST_ASSERT_FALSE(parse_command("SET:INTERVAL:3601", &c));  // >3600 非法
  TEST_ASSERT_FALSE(parse_command("SET:INTERVAL:abc", &c));   // 非数字非法
}

void test_set_key() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:KEY:sk-kimi-abcdef123456", &c));
  TEST_ASSERT_EQUAL(CMD_SET_KEY, c.type);
  TEST_ASSERT_EQUAL_STRING("sk-kimi-abcdef123456", c.key);
  TEST_ASSERT_FALSE(parse_command("SET:KEY:", &c)); // 空 key 非法
}

void test_set_wifi_password_may_contain_colons() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:WIFI:HomeWiFi:p@ss:w0rd", &c));
  TEST_ASSERT_EQUAL(CMD_SET_WIFI, c.type);
  TEST_ASSERT_EQUAL_STRING("HomeWiFi", c.ssid);
  TEST_ASSERT_EQUAL_STRING("p@ss:w0rd", c.password); // 密码按"剩余全部"取
}

void test_set_wifi_open_network_empty_password_ok() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:WIFI:CafeNet:", &c));
  TEST_ASSERT_EQUAL(CMD_SET_WIFI, c.type);
  TEST_ASSERT_EQUAL_STRING("CafeNet", c.ssid);
  TEST_ASSERT_EQUAL_STRING("", c.password);
}

void test_unknown_and_malformed() {
  Command c;
  TEST_ASSERT_FALSE(parse_command("FOOBAR", &c));
  TEST_ASSERT_FALSE(parse_command("", &c));
  TEST_ASSERT_FALSE(parse_command("SET:WIFI:onlyssid", &c)); // 缺密码段
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_no_arg_commands);
  RUN_TEST(test_set_interval_valid_and_bounds);
  RUN_TEST(test_set_key);
  RUN_TEST(test_set_wifi_password_may_contain_colons);
  RUN_TEST(test_set_wifi_open_network_empty_password_ok);
  RUN_TEST(test_unknown_and_malformed);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `.venv\Scripts\pio.exe test -e native -f test_command_parser`
Expected: FAIL，编译错误 `command_parser.h: No such file or directory`

- [ ] **Step 3: 写 `lib/core/src/command_parser.h`**

```cpp
#pragma once
#include <stdint.h>

enum CommandType : uint8_t {
  CMD_UNKNOWN = 0,
  CMD_GET_CONFIG,
  CMD_SET_WIFI,
  CMD_SET_KEY,
  CMD_SET_INTERVAL,
  CMD_REFRESH,
  CMD_GET_USAGE,
  CMD_RESET_CONFIG,
  CMD_REBOOT
};

struct Command {
  CommandType type;
  char ssid[64];
  char password[64];
  char key[128];
  long interval;
};

// 解析一行命令（不含换行）。合法返回 true 并填 *out，非法返回 false。
bool parse_command(const char* line, Command* out);
```

- [ ] **Step 4: 写 `lib/core/src/command_parser.cpp`**

```cpp
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
```

- [ ] **Step 5: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_command_parser`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/command_parser.* test/test_command_parser
git commit -m "feat(core): add serial command parser"
```

---

### Task 6: 配置校验与遮蔽 config_validate

**Files:**
- Create: `lib/core/src/config_validate.h`
- Create: `lib/core/src/config_validate.cpp`
- Test: `test/test_config_validate/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_config_validate/main.cpp`**

```cpp
#include <unity.h>
#include <string.h>
#include "config_validate.h"

void test_valid_config() {
  DeviceConfig c;
  strcpy(c.ssid, "HomeWiFi");
  strcpy(c.password, "secret");
  strcpy(c.api_key, "sk-kimi-abcdef1234567890");
  c.refresh_interval = 60;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
}

void test_empty_ssid_or_key_invalid() {
  DeviceConfig c;
  strcpy(c.ssid, "");
  strcpy(c.password, "x");
  strcpy(c.api_key, "sk-abc");
  c.refresh_interval = 60;
  TEST_ASSERT_EQUAL(CFG_ERR_NO_SSID, validate_config(&c));

  strcpy(c.ssid, "ok");
  c.api_key[0] = '\0';
  TEST_ASSERT_EQUAL(CFG_ERR_NO_KEY, validate_config(&c));
}

void test_interval_bounds() {
  DeviceConfig c;
  strcpy(c.ssid, "s"); strcpy(c.api_key, "k");
  c.refresh_interval = 29;
  TEST_ASSERT_EQUAL(CFG_ERR_BAD_INTERVAL, validate_config(&c));
  c.refresh_interval = 3601;
  TEST_ASSERT_EQUAL(CFG_ERR_BAD_INTERVAL, validate_config(&c));
  c.refresh_interval = 30;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
  c.refresh_interval = 3600;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
}

void test_open_network_password_optional() {
  DeviceConfig c;
  strcpy(c.ssid, "CafeNet");
  c.password[0] = '\0';
  strcpy(c.api_key, "k");
  c.refresh_interval = 60;
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));
}

void test_mask_api_key() {
  char buf[32];
  mask_api_key("sk-kimi-abcdef1234567890", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("sk-k...7890", buf);
  mask_api_key("short", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("****", buf);
  mask_api_key("", buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("(unset)", buf);
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_valid_config);
  RUN_TEST(test_empty_ssid_or_key_invalid);
  RUN_TEST(test_interval_bounds);
  RUN_TEST(test_open_network_password_optional);
  RUN_TEST(test_mask_api_key);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `.venv\Scripts\pio.exe test -e native -f test_config_validate`
Expected: FAIL，编译错误 `config_validate.h: No such file or directory`

- [ ] **Step 3: 写 `lib/core/src/config_validate.h`**

```cpp
#pragma once
#include <stdint.h>

struct DeviceConfig {
  char ssid[64];
  char password[64];
  char api_key[128];
  long refresh_interval; // 秒，30-3600
};

enum ConfigError : uint8_t {
  CFG_OK = 0,
  CFG_ERR_NO_SSID,
  CFG_ERR_NO_KEY,
  CFG_ERR_BAD_INTERVAL
};

ConfigError validate_config(const DeviceConfig* cfg);

// 遮蔽 API Key 用于日志/串口回显：首尾各留 4 位，中间省略；过短显示 ****，空显示 (unset)
const char* mask_api_key(const char* key, char* buf, int buf_size);
```

- [ ] **Step 4: 写 `lib/core/src/config_validate.cpp`**

```cpp
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
```

- [ ] **Step 5: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_config_validate`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/config_validate.* test/test_config_validate
git commit -m "feat(core): add config validation and key masking"
```

---

### Task 7: 退避策略 retry_policy

**Files:**
- Create: `lib/core/src/retry_policy.h`
- Create: `lib/core/src/retry_policy.cpp`
- Test: `test/test_retry_policy/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_retry_policy/main.cpp`**

```cpp
#include <unity.h>
#include "retry_policy.h"

void test_backoff_doubles_then_caps() {
  TEST_ASSERT_EQUAL_LONG(60, retry_interval_sec(60, 0));
  TEST_ASSERT_EQUAL_LONG(120, retry_interval_sec(60, 1));
  TEST_ASSERT_EQUAL_LONG(240, retry_interval_sec(60, 2));
  TEST_ASSERT_EQUAL_LONG(300, retry_interval_sec(60, 3)); // 封顶 300
  TEST_ASSERT_EQUAL_LONG(300, retry_interval_sec(60, 10));// 再高也封顶
}

void test_backoff_respects_base_and_zero_failures() {
  TEST_ASSERT_EQUAL_LONG(10, retry_interval_sec(10, 0));
  TEST_ASSERT_EQUAL_LONG(20, retry_interval_sec(10, 1));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_backoff_doubles_then_caps);
  RUN_TEST(test_backoff_respects_base_and_zero_failures);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败**

Run: `.venv\Scripts\pio.exe test -e native -f test_retry_policy`
Expected: FAIL，编译错误 `retry_policy.h: No such file or directory`

- [ ] **Step 3: 写 `lib/core/src/retry_policy.h`**

```cpp
#pragma once

// API 拉取失败后的下次重试间隔（秒）：base 起步，每次失败翻倍，封顶 300。
// failures 为当前已连续失败次数（0 表示刚失败 1 次，用 base）。
long retry_interval_sec(long base_sec, int failures);
```

- [ ] **Step 4: 写 `lib/core/src/retry_policy.cpp`**

```cpp
#include "retry_policy.h"

long retry_interval_sec(long base_sec, int failures) {
  long interval = base_sec;
  for (int i = 0; i < failures && interval < 300; i++) {
    interval *= 2;
    if (interval > 300) interval = 300;
  }
  if (interval > 300) interval = 300;
  return interval;
}
```

- [ ] **Step 5: 跑测试确认通过**

Run: `.venv\Scripts\pio.exe test -e native -f test_retry_policy`
Expected: PASS

- [ ] **Step 6: Commit + 全量回归**

```bash
.venv\Scripts\pio.exe test -e native   # 全部 6 个测试目录 PASS
git add lib/core/src/retry_policy.* test/test_retry_policy
git commit -m "feat(core): add API retry backoff policy"
```

---

### Task 8: NVS 配置存储 config_store（硬件层）

**Files:**
- Create: `src/config_store.h`
- Create: `src/config_store.cpp`

纯 NVS 读写，逻辑很薄，不值得单测；靠 Task 13 串口命令在硬件上验证。

- [ ] **Step 1: 写 `src/config_store.h`**

```cpp
#pragma once
#include "config_validate.h"

// NVS 命名空间 "cydkimi"。字段键：ssid / pass / key / interval。
bool config_store_load(DeviceConfig* cfg);        // 读取；无有效配置返回 false
bool config_store_save(const DeviceConfig* cfg);  // 整份写入
void config_store_clear();                        // 擦除并重启由调用方决定
bool config_store_is_configured();                // load + validate == CFG_OK
```

- [ ] **Step 2: 写 `src/config_store.cpp`**

```cpp
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
  p.end();

  strncpy(cfg->ssid, ssid.c_str(), sizeof(cfg->ssid) - 1);
  cfg->ssid[sizeof(cfg->ssid) - 1] = '\0';
  strncpy(cfg->password, pass.c_str(), sizeof(cfg->password) - 1);
  cfg->password[sizeof(cfg->password) - 1] = '\0';
  strncpy(cfg->api_key, key.c_str(), sizeof(cfg->api_key) - 1);
  cfg->api_key[sizeof(cfg->api_key) - 1] = '\0';
  cfg->refresh_interval = interval;
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
```

- [ ] **Step 3: 验证 esp32 环境编译通过（此时还没人调用，先把头文件加进 main.cpp 确保能编）**

在 `src/main.cpp` 顶部临时加 `#include "config_store.h"`，然后：

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`。验证后保留 include 即可（后续任务会用到）。

- [ ] **Step 4: Commit**

```bash
git add src/config_store.* src/main.cpp
git commit -m "feat(config): add NVS-backed config store"
```

---

### Task 9: 网络层 kimi_net（HTTPS fetch + NTP）

**Files:**
- Create: `src/root_ca.h`（DigiCert Global Root G2）
- Create: `src/kimi_net.h`
- Create: `src/kimi_net.cpp`

网络代码无法单测，靠 Task 13 `REFRESH`/`GET:USAGE` 在硬件上验证。关键设计：`fetch_usage` 只负责取回响应体字符串，解析交给 Task 2 的纯函数——两者解耦。

- [ ] **Step 1: 写 `src/root_ca.h`（证书内容见下，从 Windows 受信根存储导出，对应 2038-01 到期）**

```cpp
#pragma once

// DigiCert Global Root G2（api.kimi.com 证书链的根，2038-01-15 到期）
static const char* ROOT_CA_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";
```

- [ ] **Step 2: 写 `src/kimi_net.h`**

```cpp
#pragma once
#include <Arduino.h>

enum NetStatus : uint8_t {
  NET_OK = 0,
  NET_ERR_WIFI,      // WiFi 未连接
  NET_ERR_TLS,       // TLS 握手/校验失败
  NET_ERR_HTTP,      // 非 200（http_code 有值）
  NET_ERR_TIMEOUT,
  NET_ERR_BODY       // 读响应体失败
};

struct NetResult {
  NetStatus status;
  int http_code;          // NET_ERR_HTTP 时的状态码，否则 0
  bool clock_valid;       // 本次请求时 NTP 是否已对时（决定有无证书校验）
  String body;            // NET_OK 时的响应体
};

// 初始化 NTP（configTime），不阻塞。重复调用安全。
void net_time_begin();

// 阻塞等待对时，最多 timeout_ms。对时成功返回 true。
bool net_time_wait(uint32_t timeout_ms);

// 当前系统时间是否为有效 UTC（NTP 已对时）
bool net_time_valid();

// GET https://api.kimi.com/coding/v1/usages
// 对时有效 → setCACert(ROOT_CA_PEM) 严格校验；对时无效 → setInsecure() 降级。
NetResult kimi_fetch_usage(const char* api_key, uint32_t timeout_ms);
```

- [ ] **Step 3: 写 `src/kimi_net.cpp`**

```cpp
#include "kimi_net.h"
#include "root_ca.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

static const char* USAGE_URL = "https://api.kimi.com/coding/v1/usages";
static bool s_ntp_started = false;

void net_time_begin() {
  if (s_ntp_started) return;
  configTime(0, 0, "pool.ntp.org", "ntp.aliyun.com", "time.windows.com");
  s_ntp_started = true;
}

bool net_time_valid() {
  time_t now = time(nullptr);
  return now >= 1767225600; // 2026-01-01 UTC，早于它视为未对时
}

bool net_time_wait(uint32_t timeout_ms) {
  net_time_begin();
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (net_time_valid()) return true;
    delay(200);
  }
  return net_time_valid();
}

NetResult kimi_fetch_usage(const char* api_key, uint32_t timeout_ms) {
  NetResult r;
  r.status = NET_OK;
  r.http_code = 0;
  r.clock_valid = net_time_valid();
  r.body = "";

  if (WiFi.status() != WL_CONNECTED) {
    r.status = NET_ERR_WIFI;
    return r;
  }

  WiFiClientSecure client;
  if (r.clock_valid) {
    client.setCACert(ROOT_CA_PEM);
  } else {
    client.setInsecure(); // 未对时降级，屏幕用 ! 标注
  }

  HTTPClient http;
  http.setTimeout(timeout_ms);
  if (!http.begin(client, USAGE_URL)) {
    r.status = NET_ERR_TLS;
    return r;
  }
  http.addHeader("Authorization", String("Bearer ") + api_key);
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (code < 0) {
    r.status = (code == HTTPC_ERROR_READ_TIMEOUT || code == HTTPC_ERROR_CONNECTION_LOST)
                 ? NET_ERR_TIMEOUT : NET_ERR_TLS;
    http.end();
    return r;
  }
  if (code != 200) {
    r.status = NET_ERR_HTTP;
    r.http_code = code;
    http.end();
    return r;
  }

  r.body = http.getString();
  http.end();
  if (r.body.length() == 0) r.status = NET_ERR_BODY;
  return r;
}
```

- [ ] **Step 4: 在 `src/main.cpp` 加 include 验证编译**

顶部加 `#include "kimi_net.h"`，然后：

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`

- [ ] **Step 5: Commit**

```bash
git add src/root_ca.h src/kimi_net.* src/main.cpp
git commit -m "feat(net): add HTTPS client with root CA + NTP time"
```

---

### Task 10: 显示层 display（布局 B：大圆环 + 小条）

**Files:**
- Create: `src/display.h`
- Create: `src/display.cpp`

布局常量（240×320 竖屏，字号 font2 ≈16px，font4 ≈26px）：

```
y=8     标题 KIMI USAGE（font4，居中，cyan）
y=44..194  大圆环：圆心(120,119)，半径 75，环宽 14；环内百分比大字 font4
y=200   周数字 "1,360 / 2,000"（font2，居中）
y=224   周倒计时 "resets in 3d"（font2，居中，灰）
y=246   分隔线
y=254   "5H WINDOW" 标签（font2，左对齐，yellow）
y=274   细进度条：x=12..228，高 12
y=292   "60/200 · resets in 5h"（font2，居中，灰）
y=304   状态栏（font2）：WiFi 状态 + 数据年龄 + 时钟警示
```

- [ ] **Step 1: 写 `src/display.h`**

```cpp
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
```

- [ ] **Step 2: 写 `src/display.cpp`（要点：fillArc 画环、只重绘必要区域、等级→颜色映射）**

```cpp
#include "display.h"
#include <stdio.h>
#include <time.h>

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

// 画圆环：fillArc 的 seg 单位为 3 度，0 在正上方；只画到 percent 对应角度
static void draw_ring(TFT_eSPI* tft, int percent, UsageLevel lv) {
  uint16_t fg = level_color(lv);
  uint16_t track = TFT_DARKGREY;
  int segs_full = (percent * 120) / 100; // 120 段 = 360°
  // 先整环画轨道色，再覆盖前景弧
  tft->fillArc(RING_CX, RING_CY, RING_R, RING_R - RING_W, 0, 120, track, BG, true);
  if (segs_full > 0) {
    tft->fillArc(RING_CX, RING_CY, RING_R, RING_R - RING_W, 0, segs_full, fg, BG, true);
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

    char num[24];
    format_thousands(pct, num, sizeof(num));
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
    tft->setTextColor(TFT_YELLOW, BG);
    tft->drawString("5H WINDOW", 12, 258, 2);
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
```

- [ ] **Step 3: 在 `src/main.cpp` 验证编译**

顶部加 `#include "display.h"`。Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/display.* src/main.cpp
git commit -m "feat(display): add ring+bar usage renderer"
```

---

### Task 11: 配网门户 portal

**Files:**
- Create: `src/portal.h`
- Create: `src/portal.cpp`

要点：AP + DNS 劫持（DNSServer 把所有域名指到 192.168.4.1）→ 触发手机"需要登录"弹窗；中文表单；**保存前验证**（连 WiFi + 调 API，失败回显原因，不落盘）。

- [ ] **Step 1: 写 `src/portal.h`**

```cpp
#pragma once
#include <Arduino.h>
#include "config_validate.h"

struct PortalResult {
  bool submitted;      // 用户提交且验证通过
  DeviceConfig cfg;
};

// 启动 AP 模式 + WebServer。阻塞式处理 HTTP，直到验证通过或超时。
// timeout_ms 默认 10 分钟。返回 submitted=true 时调用方负责存 NVS 并重启。
PortalResult portal_run(const char* ap_name, const char* ap_pass, uint32_t timeout_ms);
```

- [ ] **Step 2: 写 `src/portal.cpp`（核心结构；HTML 为中文表单）**

```cpp
#include "portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "kimi_net.h"
#include "usage_parser.h"

static WebServer* s_server = nullptr;
static DNSServer* s_dns = nullptr;
static volatile bool s_done = false;
static DeviceConfig s_cfg;

// 验证流程：连 WiFi → NTP → fetch。返回错误说明（给用户看的中文）。
// 调用时已处于 WIFI_AP_STA 模式，这里直接 WiFi.begin 即可，AP 保持不断。
static String verify_config(const DeviceConfig& cfg) {
  WiFi.begin(cfg.ssid, cfg.password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    return "无法连接 WiFi，请检查名称和密码";
  }
  net_time_begin();
  net_time_wait(5000); // 对时失败不阻塞，fetch 会降级
  NetResult r = kimi_fetch_usage(cfg.api_key, 10000);
  if (r.status == NET_ERR_HTTP && (r.http_code == 401 || r.http_code == 403)) {
    return "API Key 无效或已失效";
  }
  if (r.status == NET_ERR_HTTP) {
    return String("服务器返回 HTTP ") + r.http_code;
  }
  if (r.status != NET_OK) {
    return "无法连接 Kimi 服务器，请检查网络";
  }
  UsageData d;
  if (parse_usage_json(r.body.c_str(), &d) != PARSE_OK) {
    return "Kimi 返回数据异常，请稍后再试";
  }
  return ""; // 成功
}

static const char* PAGE_HEAD = R"HTML(<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kimi 用量显示器配置</title>
<style>
body{font-family:system-ui,sans-serif;max-width:420px;margin:24px auto;padding:0 16px;background:#f5f5f5;color:#222}
h1{font-size:20px}
label{display:block;margin:14px 0 4px;font-weight:600}
input,select{width:100%;padding:10px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box;font-size:15px}
button{width:100%;margin-top:20px;padding:12px;background:#0078d4;color:#fff;border:0;border-radius:6px;font-size:16px}
.err{background:#fde7e9;color:#a80000;padding:10px;border-radius:6px;margin-top:14px}
.note{color:#666;font-size:13px;margin-top:6px}
</style></head><body><h1>Kimi 用量显示器配置</h1>)HTML";

// 扫描周边 WiFi，返回 <option> 列表。调用前需已处于 WIFI_AP_STA 模式。
static String scan_ssid_options() {
  int n = WiFi.scanNetworks();
  String opts;
  for (int i = 0; i < n; i++) {
    opts += "<option value=\"" + WiFi.SSID(i) + "\">";
  }
  WiFi.scanDelete();
  return opts;
}

static void send_form(const String& err) {
  String html = PAGE_HEAD;
  if (err.length()) html += "<div class='err'>" + err + "</div>";
  html += "<form method=\"POST\" action=\"/save\">\n";
  html += "<label>WiFi 名称（从列表选或手动输入）</label>\n";
  html += "<input name=\"ssid\" list=\"ssids\" required autocomplete=\"off\">\n";
  html += "<datalist id=\"ssids\">" + scan_ssid_options() + "</datalist>\n";
  html += R"HTML(<label>WiFi 密码（开放网络可留空）</label><input name="pass" type="password">
<label>Kimi API Key</label><input name="key" required placeholder="sk-...">
<label>刷新间隔（秒，30-3600，默认 60）</label><input name="interval" type="number" min="30" max="3600" value="60">
<button type="submit">保存并连接</button>
<div class="note">保存时会先验证 WiFi 和 API Key，全部通过才会写入设备。</div>
</form></body></html>)HTML";
  s_server->send(200, "text/html", html);
}

static void handle_root() { send_form(""); }

static void handle_save() {
  DeviceConfig c;
  strncpy(c.ssid, s_server->arg("ssid").c_str(), sizeof(c.ssid) - 1);
  c.ssid[sizeof(c.ssid) - 1] = '\0';
  strncpy(c.password, s_server->arg("pass").c_str(), sizeof(c.password) - 1);
  c.password[sizeof(c.password) - 1] = '\0';
  strncpy(c.api_key, s_server->arg("key").c_str(), sizeof(c.api_key) - 1);
  c.api_key[sizeof(c.api_key) - 1] = '\0';
  c.refresh_interval = s_server->arg("interval").toInt();
  if (c.refresh_interval <= 0) c.refresh_interval = 60;

  ConfigError verr = validate_config(&c);
  if (verr != CFG_OK) {
    send_form(verr == CFG_ERR_BAD_INTERVAL ? "刷新间隔需在 30-3600 秒之间" : "请完整填写 WiFi 名称和 API Key");
    return;
  }

  String err = verify_config(c);
  if (err.length()) { send_form(err); return; }

  s_cfg = c;
  s_done = true;
  s_server->send(200, "text/html",
    "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='font-family:system-ui;max-width:420px;margin:40px auto;padding:0 16px'>"
    "<h2>配置成功</h2><p>设备即将重启并开始显示用量，本热点会自动关闭。</p></body></html>");
}

static void handle_captive() { //  captive portal 探测地址统一重定向到表单
  s_server->sendHeader("Location", "http://192.168.4.1/", true);
  s_server->send(302, "text/plain", "");
}

PortalResult portal_run(const char* ap_name, const char* ap_pass, uint32_t timeout_ms) {
  PortalResult result;
  result.submitted = false;
  s_done = false;

  WiFi.mode(WIFI_AP_STA); // AP + STA：AP 提供配置页，STA 用于扫描和保存前验证
  WiFi.softAP(ap_name, ap_pass);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  s_dns = new DNSServer();
  s_dns->start(53, "*", apIP); // 所有域名劫持到设备，触发系统弹窗

  s_server = new WebServer(80);
  s_server->on("/", HTTP_GET, handle_root);
  s_server->on("/save", HTTP_POST, handle_save);
  // 常见 captive portal 探测路径
  s_server->on("/generate_204", HTTP_GET, handle_captive);        // Android
  s_server->on("/hotspot-detect.html", HTTP_GET, handle_captive); // iOS
  s_server->on("/ncsi.txt", HTTP_GET, handle_captive);            // Windows
  s_server->onNotFound(handle_captive);
  s_server->begin();

  uint32_t start = millis();
  while (!s_done && millis() - start < timeout_ms) {
    s_dns->processNextRequest();
    s_server->handleClient();
    delay(2);
  }

  if (s_done) {
    result.submitted = true;
    result.cfg = s_cfg;
    delay(1500); // 给成功页一点时间送达
  }
  s_server->stop();
  s_dns->stop();
  delete s_server; s_server = nullptr;
  delete s_dns; s_dns = nullptr;
  WiFi.softAPdisconnect(true);
  return result;
}
```

- [ ] **Step 3: 在 `src/main.cpp` 验证编译**

顶部加 `#include "portal.h"`。Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/portal.* src/main.cpp
git commit -m "feat(portal): add captive-portal config flow with pre-save verification"
```

---

### Task 12: 状态机 main.cpp（BOOT/PORTAL/CONNECTING/RUNNING）

**Files:**
- Create: `src/app_state.h`
- Modify: `src/main.cpp`（替换骨架）

- [ ] **Step 1: 写 `src/app_state.h`**

```cpp
#pragma once
#include <stdint.h>

enum AppState : uint8_t {
  STATE_BOOT = 0,
  STATE_PORTAL,
  STATE_CONNECTING,
  STATE_RUNNING
};
```

- [ ] **Step 2: 写完整 `src/main.cpp`（状态机 + 定时拉取 + 错误处理骨架；BOOT 长按在 Task 14 加）**

```cpp
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

static const char* AP_NAME = "CYD-Kimi-Setup";
static const char* AP_PASS = "kimisetup";
static const uint32_t PORTAL_TIMEOUT_MS = 10UL * 60 * 1000;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20UL * 1000;
static const uint32_t NTP_WAIT_MS = 8UL * 1000;
static const int WIFI_FAIL_TO_PORTAL = 30;

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

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("CYD Kimi Usage Ready");

  display_init(&tft);

  if (!config_store_is_configured()) {
    enter_portal(); // 不返回（内部 restart）
    return;
  }
  config_store_load(&s_cfg);
  s_next_interval_sec = s_cfg.refresh_interval;
  enter_connecting();
}

void loop() {
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
```

- [ ] **Step 3: 验证编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/app_state.h src/main.cpp
git commit -m "feat(main): wire up state machine with fetch loop and error handling"
```

---

### Task 13: 串口后门 serial_console + 交互脚本

**Files:**
- Create: `src/serial_console.h`
- Create: `src/serial_console.cpp`
- Create: `scripts/send_command.py`

把 Task 5 的 `parse_command` 接到真实动作上。为避免和 portal 状态机抢控制权，串口后门只在 `STATE_RUNNING`/`STATE_CONNECTING` 处理；`RESET:CONFIG` 等破坏性命令立即生效。

- [ ] **Step 1: 写 `src/serial_console.h`**

```cpp
#pragma once
#include <Arduino.h>

// 在 loop() 里每轮调用。内部维护行缓冲，遇到 '\n' 解析执行。
// 回调由 main.cpp 注入，避免串口模块直接碰状态机。
struct SerialHooks {
  void (*on_refresh)();
  void (*on_config_changed)(); // SET 成功后调用（重新加载配置）
  void (*on_reset_config)();   // 擦除 NVS 并重启
};

void serial_console_begin(const SerialHooks& hooks);
void serial_console_poll();
```

- [ ] **Step 2: 写 `src/serial_console.cpp`**

```cpp
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
  char masked[32];
  mask_api_key(c.api_key, masked, sizeof(masked));
  Serial.print("OK:CONFIG:{\"ssid\":\"");
  Serial.print(c.ssid);
  Serial.print("\",\"key\":\"");
  Serial.print(masked);
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
```

- [ ] **Step 3: 接到 main.cpp**

在 `main.cpp`：

```cpp
#include "serial_console.h"

static void hook_refresh() {
  if (s_state == STATE_RUNNING) fetch_and_update();
}
static void hook_config_changed() {
  config_store_load(&s_cfg);
  s_next_interval_sec = s_cfg.refresh_interval;
}
static void hook_reset_config() {
  config_store_clear();
  delay(200);
  ESP.restart();
}

// setup() 末尾、display_init 之后：
SerialHooks hooks{hook_refresh, hook_config_changed, hook_reset_config};
serial_console_begin(hooks);

// loop() 开头加：
serial_console_poll();
```

- [ ] **Step 4: 写 `scripts/send_command.py`（复用参考项目的串口交互模式）**

```python
"""向 CYD Kimi 用量显示器发送串口命令并等待 OK:/ERR: 响应。"""
import argparse
import sys
import time

import serial


def send_command(port: str, command: str, baud: int = 115200, timeout: float = 3.0) -> int:
    for attempt in range(2):
        try:
            with serial.Serial(port, baud, timeout=timeout) as ser:
                time.sleep(0.3)  # 打开串口可能触发复位
                ser.reset_input_buffer()
                ser.write((command + "\n").encode("utf-8"))
                deadline = time.time() + timeout
                while time.time() < deadline:
                    line = ser.readline().decode("utf-8", errors="replace").strip()
                    if line.startswith(("OK:", "ERR:")):
                        print(line)
                        return 0 if line.startswith("OK:") else 1
        except serial.SerialException as e:
            print(f"ERR:SERIAL:{e}", file=sys.stderr)
            return 1
    print("ERR:NO_RESPONSE", file=sys.stderr)
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Send a command to CYD Kimi usage display")
    ap.add_argument("command", help='如 "GET:CONFIG" / "SET:INTERVAL:120" / "REFRESH"')
    ap.add_argument("--port", default="COM5", help="串口，默认 COM5")
    args = ap.parse_args()
    return send_command(args.port, args.command)


if __name__ == "__main__":
    sys.exit(main())
```

用法：`.venv\Scripts\python.exe scripts\send_command.py "GET:CONFIG"`

- [ ] **Step 5: 验证编译 + Commit**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028` → `[SUCCESS]`

```bash
git add src/serial_console.* src/main.cpp scripts/send_command.py
git commit -m "feat(serial): add debug command backdoor + python client"
```

---

### Task 14: BOOT 长按重置（GPIO0）

**Files:**
- Modify: `src/main.cpp`

BOOT 键接 GPIO0，按下为低电平。在 RUNNING/CONNECTING 下检测持续按住 5 秒，屏幕倒数提示，到点擦除 NVS 重启。

- [ ] **Step 1: 在 `src/main.cpp` 加入长按检测**

顶部常量区加：

```cpp
static const uint8_t BOOT_PIN = 0;
static const uint32_t BOOT_HOLD_MS = 5000;
```

`setup()` 里 `Serial.begin` 之后加：

```cpp
pinMode(BOOT_PIN, INPUT_PULLUP);
```

新增函数并在 `loop()` 开头调用：

```cpp
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
```

`loop()` 开头（`serial_console_poll();` 之后）加 `check_boot_long_press();`

- [ ] **Step 2: 验证编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): hold BOOT 5s to wipe config and re-enter portal"
```

---

### Task 15: AGENTS.md + 硬件验证清单 + 收尾

**Files:**
- Create: `AGENTS.md`

- [ ] **Step 1: 写 `AGENTS.md`（烧录流程沿用参考项目经验 + 手工验证清单）**

````markdown
# Agent Notes

ESP32-2432S028（CYD）Kimi Coding Plan 用量显示器。设备连 WiFi 直连 Kimi API，配置通过手机配网页面录入并存在 NVS。

## 环境与命令

- Python 环境用 uv 管理：`uv sync` 后 pio 在 `.venv\Scripts\pio.exe`
- 编译：`.venv\Scripts\pio.exe run -e esp32-2432s028`
- 主机端单元测试（纯逻辑，不烧硬件）：`.venv\Scripts\pio.exe test -e native`
  - 需要 g++（MinGW）。没有就 `scoop install mingw-winlibs`
- 串口后门：`.venv\Scripts\python.exe scripts\send_command.py "GET:CONFIG"`

## 烧录特别注意（实操经验）

这块板子自动下载电路不起作用，标准流程：

1. 先按住 BOOT 按钮不松开
2. 运行 `.venv\Scripts\pio.exe run --target upload`
3. 看到 `Serial port COM5` 并打印 `Connecting.....` 时松开 BOOT
4. 等待 `[SUCCESS]`
5. 上传成功后按一下 RST（EN）让芯片进入正常运行模式

不按 RST 的话串口后门可能收不到 `OK:` 响应。

## 配网流程

1. 首次上电（或配置被擦除）进入 Setup 模式，屏幕显示热点名和密码
2. 手机连热点 `CYD-Kimi-Setup`（密码 `kimisetup`），系统弹出配置页（或手动开 192.168.4.1）
3. 填 WiFi、Kimi API Key、刷新间隔，保存
4. 设备先验证 WiFi + API Key，通过才写入并重启

## 串口后门（115200，`\n` 结尾）

| 命令 | 作用 |
|---|---|
| `GET:CONFIG` | 查看配置（key 遮蔽） |
| `SET:WIFI:<ssid>:<pass>` | 改 WiFi |
| `SET:KEY:<apikey>` | 改 API Key |
| `SET:INTERVAL:<30-3600>` | 改刷新间隔 |
| `REFRESH` | 立即拉取 |
| `GET:USAGE` | 提示看屏幕 |
| `RESET:CONFIG` | 擦除配置并重启 |
| `REBOOT` | 重启 |

## 手工验证清单

- [ ] 首次上电进入 Setup 模式，屏幕显示热点信息
- [ ] 手机连热点弹出配置页，提交错误密码回显"无法连接 WiFi"
- [ ] 提交错误 API Key 回显"API Key 无效"
- [ ] 正确提交后重启，进入用量显示页（大圆环 + 5H 条）
- [ ] 断路由器：状态栏 WiFi LOST，数据转灰并显示年龄，恢复后自动回来
- [ ] 断电重启：自动连 WiFi 并显示，不用重配
- [ ] 按住 BOOT 5 秒：屏幕倒数，松手取消，按住到底则擦除配置重启进 Setup
- [ ] 串口 `GET:CONFIG` 返回遮蔽后的 key；`SET:INTERVAL:120` 生效
- [ ] 用量显示数字与桌面端工具一致

## 目录说明

- `lib/core/`：纯逻辑，native 可测（解析/格式化/配色/命令/校验/退避）
- `src/`：硬件层（display / kimi_net / portal / config_store / serial_console / main）
- `scripts/`：Windows 编译修复 + 串口交互脚本
````

- [ ] **Step 2: 全量回归**

```powershell
.venv\Scripts\pio.exe test -e native          # 全部 PASS
.venv\Scripts\pio.exe run -e esp32-2432s028   # [SUCCESS]
```

- [ ] **Step 3: Commit**

```bash
git add AGENTS.md
git commit -m "docs: add AGENTS.md with flash/portal/serial guide and hardware checklist"
```

---

## 执行顺序与依赖

```
Task 0 脚手架
 └─ Task 1-7（core 纯逻辑，TDD，可并行开发但按序提交）
 └─ Task 8-11（硬件层薄封装，各自能编过）
 └─ Task 12（状态机，依赖 8/9/10/11）
 └─ Task 13（串口，依赖 5/8/12）
 └─ Task 14（BOOT 长按，依赖 12）
 └─ Task 15（文档收尾）
```

**硬件烧录验证点**：Task 12 完成后第一次烧录验证配网 + 显示主流程；Task 13 后验证串口后门；Task 14 后验证长按重置；Task 15 清单全部走一遍。
