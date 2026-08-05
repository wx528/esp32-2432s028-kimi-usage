# 显示方向旋转（4 方向 + NVS 持久化）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 支持 0°/90°/180°/270° 四个显示方向；运行中短按 BOOT 循环切换，立即生效并写 NVS，重启保持。

**Architecture:** display.cpp 内置竖屏/横屏双布局表（Layout struct），`display_init`/`display_rotate` 接收方向并存 static；竖屏布局一个像素不动，180° 靠 setRotation 硬件翻转白送。`DeviceConfig` 加 `rotation` 字段，NVS 键 `rot` 默认 0、脏数据兜底。BOOT 短按（<500ms 松开）走 `rotation_next` 循环。

**Tech Stack:** 同现有项目（无新依赖）。native 单测 MinGW。

**Spec:** `docs/superpowers/specs/2026-08-05-display-rotation-design.md`

**项目根目录:** 本仓库根目录（下称 `$root`）。pio 为 `.venv\Scripts\pio.exe`。**native 测试前必须：** `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path`

## 文件结构

```
lib/core/src/display_model.{h,cpp}  Task 1：rotation_next
test/test_display_model/main.cpp    Task 1：追加测试
lib/core/src/config_validate.h      Task 2：DeviceConfig 加 rotation
src/config_store.cpp                Task 2：NVS rot 键
src/display.{h,cpp}                 Task 3：双布局表 + display_rotate + 静态页相对居中
src/main.cpp                        Task 4：s_rotation、setup 顺序、BOOT 短按、擦除提示坐标方向感知
README.md / README.zh-CN.md / AGENTS.md / CHANGELOG.md  Task 5
```

约定：rotation 取值 0-3 与 TFT_eSPI `setRotation` 参数一致（0=竖屏、1=横屏、2=竖屏翻转、3=横屏翻转）。

---

### Task 1: rotation_next（纯逻辑，TDD）

**Files:**
- Modify: `lib/core/src/display_model.h`
- Modify: `lib/core/src/display_model.cpp`
- Test: `test/test_display_model/main.cpp`（追加）

- [ ] **Step 1: 追加失败测试（并注册 RUN_TEST）**

```cpp
void test_rotation_next() {
  TEST_ASSERT_EQUAL_UINT8(1, rotation_next(0));
  TEST_ASSERT_EQUAL_UINT8(2, rotation_next(1));
  TEST_ASSERT_EQUAL_UINT8(3, rotation_next(2));
  TEST_ASSERT_EQUAL_UINT8(0, rotation_next(3)); // 循环
  TEST_ASSERT_EQUAL_UINT8(0, rotation_next(4)); // 越界 → 0
  TEST_ASSERT_EQUAL_UINT8(0, rotation_next(7));
  TEST_ASSERT_EQUAL_UINT8(0, rotation_next(255));
}
```

- [ ] **Step 2: 跑测试确认失败（编译错误：rotation_next 未声明）**

Run: `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native -f test_display_model`

- [ ] **Step 3: display_model.h 加声明**

```cpp
// 显示方向循环：0→1→2→3→0；非法输入（>=3 以外）归 0
uint8_t rotation_next(uint8_t r);
```

- [ ] **Step 4: display_model.cpp 加实现**

```cpp
uint8_t rotation_next(uint8_t r) {
  return r < 3 ? r + 1 : 0;
}
```

- [ ] **Step 5: 跑测试确认通过（旧 2 + 新 1）**

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/display_model.* test/test_display_model
git commit -m "feat(core): add rotation_next cycle helper"
```

---

### Task 2: DeviceConfig 加 rotation + NVS 持久化

**Files:**
- Modify: `lib/core/src/config_validate.h`
- Modify: `src/config_store.cpp`

`validate_config` **不改**（rotation 不来自配网页/串口，load 兜底即可）。现有 native 测试不需要动（校验不读 rotation）。

- [ ] **Step 1: config_validate.h 的 DeviceConfig 末尾加字段**

```cpp
  uint8_t provider_mode;  // ProviderMode
  uint8_t rotation;       // 显示方向 0-3（TFT_eSPI setRotation 参数）
```

- [ ] **Step 2: config_store.cpp**

load：在 `mode` 读取后加：

```cpp
  uint8_t rot = p.getUChar("rot", 0);
```

在 `provider_mode` 赋值后加：

```cpp
  cfg->rotation = rot > 3 ? 0 : rot; // 脏数据兜底
```

save：在 `p.putUChar("mode", ...)` 后加：

```cpp
  p.putUChar("rot", cfg->rotation);
```

- [ ] **Step 3: 验证 esp32 编译 + native 全量**

```powershell
.venv\Scripts\pio.exe run -e esp32-2432s028   # [SUCCESS]
$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native   # 全 PASS
```

- [ ] **Step 4: Commit**

```bash
git add lib/core/src/config_validate.h src/config_store.cpp
git commit -m "feat(config): persist display rotation in NVS"
```

---

### Task 3: display 双布局表 + display_rotate + 静态页相对居中

**Files:**
- Modify: `src/display.h`
- Modify: `src/display.cpp`

- [ ] **Step 1: display.h 改**

`display_init` 加 rotation 参数；新增 `display_rotate`：

```cpp
void display_init(TFT_eSPI* tft, uint8_t rotation);
void display_rotate(TFT_eSPI* tft, uint8_t rotation); // 运行中切换方向（立即 setRotation）
```

- [ ] **Step 2: display.cpp 整体替换为以下内容**

```cpp
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
```

- [ ] **Step 3: 验证 esp32 编译（此时 main.cpp 的 display_init 调用还没改，会编译失败——见下）**

`display_init` 签名变了，main.cpp `display_init(&tft);` 会报错。**本任务同时把 main.cpp 的调用改为 `display_init(&tft, 0);`**（占位，Task 4 换成真实方向），保证可编译：

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028` → `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/display.* src/main.cpp
git commit -m "feat(display): dual portrait/landscape layouts + runtime rotate"
```

---

### Task 4: main.cpp 接线（s_rotation、setup 顺序、BOOT 短按、擦除提示方向感知）

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: 全局加 s_rotation**

`static uint8_t s_active = ...` 附近加：

```cpp
static uint8_t s_rotation = 0; // 显示方向 0-3
```

- [ ] **Step 2: setup() 顺序调整**

现状：`display_init(&tft, 0);` 在配置加载之前。改为**先 load 配置拿 rotation，再 display_init**：

```cpp
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
```

（即：把 `display_init` 移到 load 之后；删掉后面重复的 `config_store_load(&s_cfg);`。）

- [ ] **Step 3: cycle_rotation + BOOT 短按**

新增函数（放在 `check_boot_long_press` 前）：

```cpp
static void cycle_rotation() {
  s_rotation = rotation_next(s_rotation);
  s_cfg.rotation = s_rotation;
  config_store_save(&s_cfg);
  Serial.printf("OK:ROTATION:%d\n", s_rotation * 90);
  display_rotate(&tft, s_rotation);
  s_touch.setRotation(s_rotation);
  redraw();
}
```

`check_boot_long_press` 的松开分支改为：

```cpp
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
```

- [ ] **Step 4: 擦除倒数与 "Config erased" 坐标方向感知**

倒数提示（held > 500 分支）改为按屏幕尺寸定位：

```cpp
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
```

擦除成功页改居中：

```cpp
      tft.drawString("Config erased", tft.width() / 2, tft.height() / 2, 2);
```

（原 `(120, 150)` 和 `(120, 290)` / `fillRect(0,280,240,20,...)` 全部替换——横屏下旧坐标在屏幕外。）

- [ ] **Step 5: 验证 esp32 编译 + native 全量回归**

```powershell
.venv\Scripts\pio.exe run -e esp32-2432s028   # [SUCCESS]
$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native   # 全 PASS（46 用例）
```

- [ ] **Step 6: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): BOOT short-press cycles display rotation, persisted in NVS"
```

---

### Task 5: 回归 + 文档

**Files:**
- Modify: `README.md`, `README.zh-CN.md`, `AGENTS.md`, `CHANGELOG.md`

- [ ] **Step 1: 全量回归**

```powershell
$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native
.venv\Scripts\pio.exe run -e esp32-2432s028
```

- [ ] **Step 2: 文档**

- `README.md` Features 加：
  ```
  - Four display orientations (0/90/180/270): short-press BOOT to cycle, persisted across reboots
  ```
  `README.zh-CN.md` 功能加：
  ```
  - 四个显示方向（0/90/180/270）：短按 BOOT 循环切换，断电记忆
  ```
  两个 README 的"重新配置 / Changing Configuration"一节各加一句：短按 BOOT（<0.5 秒）切换显示方向，长按 5 秒仍是擦除配置。
- `AGENTS.md`：烧录/串口两节之间的合适位置补一句交互说明（短按=旋转、点击屏幕=切 provider、长按 5 秒=擦除）；手工验证清单追加：
  ```
  - [ ] 短按 BOOT：方向循环 0→90→180→270→0，每次立即重绘
  - [ ] 横屏布局各元素位置正常（左环右栏、无重叠无越界）
  - [ ] 切换方向后断电重启：保持新方向
  - [ ] 旧配置升级：默认 0° 竖屏，行为不变
  - [ ] 短按不误触擦除；长按 5 秒擦除功能不变
  ```
- `CHANGELOG.md` 顶部 `## [Unreleased]` 段加（若无该段则新建）：
  ```
  ### Added

  - Four display orientations (0/90/180/270) with a dedicated landscape layout (ring left, info right); short-press BOOT to cycle, persisted in NVS across reboots
  - CI: GitHub Actions workflow running native unit tests + esp32 firmware build on push/PR
  ```

  （CI 一项补记上一迭代的遗漏。）

- [ ] **Step 3: Commit**

```bash
git add README.md README.zh-CN.md AGENTS.md CHANGELOG.md
git commit -m "docs: document display rotation + CI"
```

---

## 执行顺序与依赖

```
Task 1（rotation_next，纯逻辑 TDD）
Task 2（config/NVS，依赖 Task 1 无，但先行以便 Task 4 使用）
Task 3（display 双布局；含 main.cpp 占位调用保证可编）
Task 4（main 接线，依赖 Task 1/2/3）
Task 5（回归 + 文档）
```

**硬件烧录验证点**：Task 4 完成后烧录，按 Task 5 清单验证（短按循环、横屏布局、断电记忆、旧配置无感、长按擦除不受干扰）。
