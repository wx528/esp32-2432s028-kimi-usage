# 多 Provider 支持（Kimi + MiniMax）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 同一台 CYD 设备可配置只看 Kimi、只看 MiniMax 或两家都看；both 模式点击触摸屏切换显示，定时只拉激活 provider，切换时立即拉取新激活的一家。

**Architecture:** 在现有单 provider 固件上加 provider 抽象：`lib/core` 新增 `provider.h`（名称/URL/parse 分发）与 `minimax_parser`（纯逻辑，native 可测）；`DeviceConfig` 加 `provider_mode` + `minimax_key`；main.cpp 改双 RAM 槽（每 provider 一份缓存与错误状态）；触摸用 XPT2046_Touchscreen 库的 `touched()`（全屏热区，无需坐标校准）。MiniMax 的 `used = 100 - remaining_percent`、`limit = 100`、毫秒 epoch 重置，都映射进现有 `UsageData`，显示层仅标题参数化。

**Tech Stack:** 同现有项目（PlatformIO + Arduino、TFT_eSPI、ArduinoJson 7）+ paulstoffregen/XPT2046_Touchscreen @ ^1.4 · native 单测 MinGW

**Spec:** `docs/superpowers/specs/2026-08-05-multi-provider-design.md`

**项目根目录:** 本仓库根目录（下称 `$root`）。所有命令在 `$root` 下执行。pio 为 `.venv\Scripts\pio.exe`。**native 测试前必须：** `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path`

**Unity 宏注意：** PlatformIO 自带 Unity 没有 `TEST_ASSERT_EQUAL_LONG`，一律用 `TEST_ASSERT_EQUAL_INT32`。

## 文件结构

```
lib/core/src/
  ├── usage_types.h          Task 2：ParseResult 加 PARSE_ERR_API
  ├── time_parse.{h,cpp}     Task 1：加 ms_epoch_to_sec
  ├── minimax_parser.{h,cpp} Task 2：新 parser（纯）
  ├── provider.{h,cpp}       Task 3：Provider 枚举 + name/url/parse 分发（纯）
  ├── config_validate.{h,cpp} Task 4：ProviderMode + minimax_key + CFG_ERR_BAD_MODE
  └── command_parser.{h,cpp} Task 5：CMD_SET_PROVIDER / CMD_SET_MMKEY
src/
  ├── config_store.cpp       Task 6：NVS mode/mmkey
  ├── root_ca.h              Task 7：MINIMAX_ROOT_CA_PEM（USERTrust RSA，2038-01 到期）
  ├── kimi_net.{h,cpp}       Task 7：net_fetch_usage(Provider,...)，kimi_fetch_usage 保留为包装
  ├── display.{h,cpp}        Task 8：DisplayState 加 title/switch_hint
  ├── portal.cpp             Task 9：表单加 provider 选择 + mmkey + 逐家验证
  ├── main.cpp               Task 10：双槽状态机 + 触摸切换
  └── serial_console.cpp     Task 11：新命令 + GET:CONFIG 扩展
test/
  ├── test_time_parse/main.cpp        Task 1 追加
  ├── test_minimax_parser/main.cpp    Task 2 新建
  ├── test_provider/main.cpp          Task 3 新建
  ├── test_config_validate/main.cpp   Task 4 修改（旧用例补 provider_mode）
  └── test_command_parser/main.cpp    Task 5 追加
platformio.ini               Task 10：lib_deps 加 XPT2046_Touchscreen
```

约定：MiniMax 的毫秒 epoch 用 `int64_t`（ESP32 的 long 是 32 位，装不下毫秒）。provider 在结构体/配置里用 `uint8_t` 存储（0=kimi/1=minimax/2=both），core 里枚举名 `ProviderMode { MODE_KIMI, MODE_MINIMAX, MODE_BOTH }` 放 `config_validate.h`，`Provider { PROVIDER_KIMI, PROVIDER_MINIMAX }` 放 `provider.h`，两者值一一对应。

---

### Task 1: time_parse 加 ms_epoch_to_sec（毫秒 epoch → 秒）

**Files:**
- Modify: `lib/core/src/time_parse.h`
- Modify: `lib/core/src/time_parse.cpp`
- Test: `test/test_time_parse/main.cpp`（追加）

- [ ] **Step 1: 追加失败测试**

在 `test/test_time_parse/main.cpp` 加（并把 RUN_TEST 加进 main）：

```cpp
void test_ms_epoch_to_sec() {
  TEST_ASSERT_EQUAL_UINT32(1785772800UL, ms_epoch_to_sec(1785772800000LL));
  TEST_ASSERT_EQUAL_UINT32(1786291200UL, ms_epoch_to_sec(1786291200000LL));
}

void test_ms_epoch_to_sec_nonpositive_is_zero() {
  TEST_ASSERT_EQUAL_UINT32(0UL, ms_epoch_to_sec(0));
  TEST_ASSERT_EQUAL_UINT32(0UL, ms_epoch_to_sec(-5));
}
```

- [ ] **Step 2: 跑测试确认失败（编译错误：ms_epoch_to_sec 未声明）**

Run: `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native -f test_time_parse`

- [ ] **Step 3: time_parse.h 加声明**

```cpp
// 毫秒 epoch（UTC）转秒。<=0 返回 0。
uint32_t ms_epoch_to_sec(int64_t ms);
```

（`int64_t` 已在 `<stdint.h>`。）

- [ ] **Step 4: time_parse.cpp 加实现**

```cpp
uint32_t ms_epoch_to_sec(int64_t ms) {
  if (ms <= 0) return 0;
  return (uint32_t)(ms / 1000);
}
```

- [ ] **Step 5: 跑测试确认通过（含旧用例共 6 个）**

Run: 同 Step 2，Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/time_parse.* test/test_time_parse
git commit -m "feat(core): add ms-epoch to seconds conversion"
```

---

### Task 2: minimax_parser（JSON → UsageData）+ PARSE_ERR_API

**Files:**
- Modify: `lib/core/src/usage_types.h`
- Create: `lib/core/src/minimax_parser.h`
- Create: `lib/core/src/minimax_parser.cpp`
- Test: `test/test_minimax_parser/main.cpp`

- [ ] **Step 1: usage_types.h 的 ParseResult 末尾追加**

```cpp
  PARSE_ERR_KEY_DISABLED, // boosterWallet.status == STATUS_DISABLED
  PARSE_ERR_API           // 提供商业务层报错（如 MiniMax base_resp.status_code != 0）
};
```

- [ ] **Step 2: 写失败测试 `test/test_minimax_parser/main.cpp`**

样本为桌面端项目验证过的真实响应（count 字段恒 0，以 percent 为准）：

```cpp
#include <unity.h>
#include "minimax_parser.h"

static const char* SAMPLE = R"json({
  "model_remains": [
    {
      "start_time": 1785758400000,
      "end_time": 1785772800000,
      "remains_time": 5761296,
      "current_interval_total_count": 0,
      "current_interval_usage_count": 0,
      "model_name": "general",
      "current_weekly_total_count": 0,
      "current_weekly_usage_count": 0,
      "weekly_start_time": 1785686400000,
      "weekly_end_time": 1786291200000,
      "weekly_remains_time": 524161296,
      "current_interval_status": 1,
      "current_weekly_status": 1,
      "current_interval_remaining_percent": 98,
      "current_weekly_remaining_percent": 87,
      "weekly_boost_permille": 1500
    }
  ],
  "base_resp": {"status_code": 0, "status_msg": "success"}
})json";

void test_parse_percent_fields() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_minimax_json(SAMPLE, &d));
  TEST_ASSERT_EQUAL_INT32(13, d.plan_used);      // 100 - 87
  TEST_ASSERT_EQUAL_INT32(100, d.plan_limit);
  TEST_ASSERT_EQUAL_UINT32(1786291200UL, d.plan_reset);
  TEST_ASSERT_EQUAL_INT32(2, d.window_used);     // 100 - 98
  TEST_ASSERT_EQUAL_INT32(100, d.window_limit);
  TEST_ASSERT_EQUAL_UINT32(1785772800UL, d.window_reset);
}

void test_counts_are_ignored() {
  // count 字段即使非 0 也不影响结果（MiniMax 转 token 计量后恒 0，percent 才权威）
  const char* j = R"json({
    "model_remains":[{"model_name":"general","current_weekly_total_count":999,
      "current_weekly_usage_count":999,"current_interval_total_count":999,"current_interval_usage_count":999,
      "current_weekly_remaining_percent":40,"current_interval_remaining_percent":60,
      "weekly_end_time":1786291200000,"end_time":1785772800000}],
    "base_resp":{"status_code":0,"status_msg":"success"}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, parse_minimax_json(j, &d));
  TEST_ASSERT_EQUAL_INT32(60, d.plan_used);
  TEST_ASSERT_EQUAL_INT32(40, d.window_used);
}

void test_base_resp_error_is_api_error() {
  const char* j = R"json({"model_remains":[],"base_resp":{"status_code":1,"status_msg":"unauthorized"}})json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_API, parse_minimax_json(j, &d));
}

void test_missing_general_entry_is_missing() {
  const char* j = R"json({"model_remains":[{"model_name":"video"}],"base_resp":{"status_code":0}})json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_MISSING, parse_minimax_json(j, &d));
}

void test_invalid_json_is_json_error() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_JSON, parse_minimax_json("not json", &d));
}

void test_percent_out_of_range_is_bad_value() {
  const char* j = R"json({
    "model_remains":[{"model_name":"general","current_weekly_remaining_percent":150,
      "current_interval_remaining_percent":50,"weekly_end_time":1786291200000,"end_time":1785772800000}],
    "base_resp":{"status_code":0}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_BAD_VALUE, parse_minimax_json(j, &d));
}

void test_zero_reset_is_bad_value() {
  const char* j = R"json({
    "model_remains":[{"model_name":"general","current_weekly_remaining_percent":50,
      "current_interval_remaining_percent":50,"weekly_end_time":0,"end_time":1785772800000}],
    "base_resp":{"status_code":0}
  })json";
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_BAD_VALUE, parse_minimax_json(j, &d));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_parse_percent_fields);
  RUN_TEST(test_counts_are_ignored);
  RUN_TEST(test_base_resp_error_is_api_error);
  RUN_TEST(test_missing_general_entry_is_missing);
  RUN_TEST(test_invalid_json_is_json_error);
  RUN_TEST(test_percent_out_of_range_is_bad_value);
  RUN_TEST(test_zero_reset_is_bad_value);
  return UNITY_END();
}
```

- [ ] **Step 3: 跑测试确认失败（编译错误：minimax_parser.h 不存在）**

Run: `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native -f test_minimax_parser`

- [ ] **Step 4: 写 `lib/core/src/minimax_parser.h`**

```cpp
#pragma once
#include "usage_types.h"

// 解析 MiniMax /v1/token_plan/remains 响应体为 UsageData。
// 取 model_remains[] 中 model_name=="general" 的条目；用量以 remaining_percent 为准
//（count 字段自 MiniMax 转 token 计量后恒 0），limit 恒 100；重置时间为毫秒 epoch。
// 成功返回 PARSE_OK 并写入 *out；失败返回对应 ParseResult，out 不被修改。
ParseResult parse_minimax_json(const char* json, UsageData* out);
```

- [ ] **Step 5: 写 `lib/core/src/minimax_parser.cpp`**

```cpp
#include "minimax_parser.h"
#include "time_parse.h"
#include <ArduinoJson.h>
#include <string.h>

ParseResult parse_minimax_json(const char* json, UsageData* out) {
  if (!json || !out) return PARSE_ERR_MISSING;

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return PARSE_ERR_JSON;

  JsonVariantConst br = doc["base_resp"];
  if (!br.isNull() && br["status_code"].as<long>() != 0) return PARSE_ERR_API;

  JsonArrayConst arr = doc["model_remains"].as<JsonArrayConst>();
  if (arr.isNull()) return PARSE_ERR_MISSING;

  JsonVariantConst general;
  for (JsonVariantConst item : arr) {
    if (strcmp(item["model_name"] | "", "general") == 0) { general = item; break; }
  }
  if (general.isNull()) return PARSE_ERR_MISSING;

  if (!general["current_weekly_remaining_percent"].is<long>()) return PARSE_ERR_BAD_VALUE;
  if (!general["current_interval_remaining_percent"].is<long>()) return PARSE_ERR_BAD_VALUE;
  long wp = general["current_weekly_remaining_percent"].as<long>();
  long ip = general["current_interval_remaining_percent"].as<long>();
  if (wp < 0 || wp > 100 || ip < 0 || ip > 100) return PARSE_ERR_BAD_VALUE;

  uint32_t plan_reset = ms_epoch_to_sec(general["weekly_end_time"].as<int64_t>());
  uint32_t win_reset = ms_epoch_to_sec(general["end_time"].as<int64_t>());
  if (plan_reset == 0 || win_reset == 0) return PARSE_ERR_BAD_VALUE;

  out->plan_used = 100 - wp;
  out->plan_limit = 100;
  out->plan_reset = plan_reset;
  out->window_used = 100 - ip;
  out->window_limit = 100;
  out->window_reset = win_reset;
  return PARSE_OK;
}
```

- [ ] **Step 6: 跑测试确认通过（7 个用例）**

Run: 同 Step 3，Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add lib/core/src/usage_types.h lib/core/src/minimax_parser.* test/test_minimax_parser
git commit -m "feat(core): add MiniMax usage JSON parser"
```

---

### Task 3: provider 抽象（name / url / parse 分发）

**Files:**
- Create: `lib/core/src/provider.h`
- Create: `lib/core/src/provider.cpp`
- Test: `test/test_provider/main.cpp`

- [ ] **Step 1: 写失败测试 `test/test_provider/main.cpp`**

```cpp
#include <unity.h>
#include <string.h>
#include "provider.h"

static const char* KIMI_JSON = R"json({
  "usage": {"limit":"100","used":"89","resetTime":"2026-07-21T06:38:42Z"},
  "limits":[{"detail":{"limit":"100","used":"7","resetTime":"2026-07-17T20:38:42Z"}}]
})json";

static const char* MINIMAX_JSON = R"json({
  "model_remains":[{"model_name":"general","current_weekly_remaining_percent":87,
    "current_interval_remaining_percent":98,"weekly_end_time":1786291200000,"end_time":1785772800000}],
  "base_resp":{"status_code":0,"status_msg":"success"}
})json";

void test_provider_names_and_urls() {
  TEST_ASSERT_EQUAL_STRING("KIMI", provider_name(PROVIDER_KIMI));
  TEST_ASSERT_EQUAL_STRING("MINIMAX", provider_name(PROVIDER_MINIMAX));
  TEST_ASSERT_EQUAL_STRING("https://api.kimi.com/coding/v1/usages", provider_url(PROVIDER_KIMI));
  TEST_ASSERT_EQUAL_STRING("https://www.minimaxi.com/v1/token_plan/remains", provider_url(PROVIDER_MINIMAX));
}

void test_parse_dispatches_to_kimi() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, provider_parse(PROVIDER_KIMI, KIMI_JSON, &d));
  TEST_ASSERT_EQUAL_INT32(89, d.plan_used);
}

void test_parse_dispatches_to_minimax() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_OK, provider_parse(PROVIDER_MINIMAX, MINIMAX_JSON, &d));
  TEST_ASSERT_EQUAL_INT32(13, d.plan_used);
}

void test_cross_parse_fails_cleanly() {
  UsageData d;
  TEST_ASSERT_EQUAL(PARSE_ERR_MISSING, provider_parse(PROVIDER_KIMI, MINIMAX_JSON, &d));
}

int main(int argc, char** argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_provider_names_and_urls);
  RUN_TEST(test_parse_dispatches_to_kimi);
  RUN_TEST(test_parse_dispatches_to_minimax);
  RUN_TEST(test_cross_parse_fails_cleanly);
  return UNITY_END();
}
```

- [ ] **Step 2: 跑测试确认失败（编译错误：provider.h 不存在）**

Run: `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native -f test_provider`

- [ ] **Step 3: 写 `lib/core/src/provider.h`**

```cpp
#pragma once
#include <stdint.h>
#include "usage_types.h"

enum Provider : uint8_t {
  PROVIDER_KIMI = 0,
  PROVIDER_MINIMAX
};

const char* provider_name(Provider p);  // "KIMI" / "MINIMAX"，屏幕标题用
const char* provider_url(Provider p);   // API endpoint，net 层用

// 按 provider 分发到对应 parser。
ParseResult provider_parse(Provider p, const char* json, UsageData* out);
```

- [ ] **Step 4: 写 `lib/core/src/provider.cpp`**

```cpp
#include "provider.h"
#include "usage_parser.h"
#include "minimax_parser.h"

const char* provider_name(Provider p) {
  return p == PROVIDER_MINIMAX ? "MINIMAX" : "KIMI";
}

const char* provider_url(Provider p) {
  return p == PROVIDER_MINIMAX
           ? "https://www.minimaxi.com/v1/token_plan/remains"
           : "https://api.kimi.com/coding/v1/usages";
}

ParseResult provider_parse(Provider p, const char* json, UsageData* out) {
  return p == PROVIDER_MINIMAX
           ? parse_minimax_json(json, out)
           : parse_usage_json(json, out);
}
```

- [ ] **Step 5: 跑测试确认通过（4 个用例）**

Run: 同 Step 2，Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add lib/core/src/provider.* test/test_provider
git commit -m "feat(core): add provider dispatch (name/url/parse)"
```

---

### Task 4: config_validate 加 ProviderMode + minimax_key

**Files:**
- Modify: `lib/core/src/config_validate.h`
- Modify: `lib/core/src/config_validate.cpp`
- Test: `test/test_config_validate/main.cpp`（旧用例补 `provider_mode`，加新用例）

- [ ] **Step 1: 改 `config_validate.h`**

```cpp
#pragma once
#include <stdint.h>

enum ProviderMode : uint8_t {
  MODE_KIMI = 0,
  MODE_MINIMAX,
  MODE_BOTH
};

struct DeviceConfig {
  char ssid[64];
  char password[64];
  char api_key[128];      // Kimi
  char minimax_key[128];  // MiniMax
  long refresh_interval;  // 秒，30-3600
  uint8_t provider_mode;  // ProviderMode
};

enum ConfigError : uint8_t {
  CFG_OK = 0,
  CFG_ERR_NO_SSID,
  CFG_ERR_NO_KEY,
  CFG_ERR_BAD_INTERVAL,
  CFG_ERR_BAD_MODE
};

ConfigError validate_config(const DeviceConfig* cfg);

// 遮蔽 API Key 用于日志/串口回显：首尾各留 4 位，中间省略；过短显示 ****，空显示 (unset)
const char* mask_api_key(const char* key, char* buf, int buf_size);
```

- [ ] **Step 2: 改 `config_validate.cpp` 的 validate_config（mask_api_key 不动）**

```cpp
ConfigError validate_config(const DeviceConfig* cfg) {
  if (!cfg) return CFG_ERR_NO_SSID;
  if (cfg->ssid[0] == '\0') return CFG_ERR_NO_SSID;
  if (cfg->provider_mode > MODE_BOTH) return CFG_ERR_BAD_MODE;
  if (cfg->provider_mode != MODE_MINIMAX && cfg->api_key[0] == '\0') return CFG_ERR_NO_KEY;
  if (cfg->provider_mode != MODE_KIMI && cfg->minimax_key[0] == '\0') return CFG_ERR_NO_KEY;
  if (cfg->refresh_interval < 30 || cfg->refresh_interval > 3600) return CFG_ERR_BAD_INTERVAL;
  return CFG_OK;
}
```

- [ ] **Step 3: 改测试**

旧 5 个用例里的每个 `DeviceConfig c;` 之后补两行（否则新校验会失败）：

```cpp
  c.minimax_key[0] = '\0';
  c.provider_mode = MODE_KIMI;
```

追加新用例（并注册进 main）：

```cpp
void test_provider_mode_matrix() {
  DeviceConfig c;
  strcpy(c.ssid, "s");
  c.api_key[0] = '\0';
  c.minimax_key[0] = '\0';
  c.refresh_interval = 60;

  c.provider_mode = MODE_KIMI;
  TEST_ASSERT_EQUAL(CFG_ERR_NO_KEY, validate_config(&c)); // 缺 kimi key
  strcpy(c.api_key, "k");
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));         // kimi 模式不需要 mmkey

  c.provider_mode = MODE_MINIMAX;
  TEST_ASSERT_EQUAL(CFG_ERR_NO_KEY, validate_config(&c)); // 缺 mmkey
  strcpy(c.minimax_key, "m");
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));         // minimax 模式不需要 kimi key
  c.api_key[0] = '\0';
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));

  c.provider_mode = MODE_BOTH;
  TEST_ASSERT_EQUAL(CFG_ERR_NO_KEY, validate_config(&c)); // 两把都要
  strcpy(c.api_key, "k");
  TEST_ASSERT_EQUAL(CFG_OK, validate_config(&c));

  c.provider_mode = 3;
  TEST_ASSERT_EQUAL(CFG_ERR_BAD_MODE, validate_config(&c));
}
```

- [ ] **Step 4: 跑测试确认通过（旧 5 + 新 1）**

Run: `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native -f test_config_validate`

- [ ] **Step 5: Commit**

```bash
git add lib/core/src/config_validate.* test/test_config_validate
git commit -m "feat(core): add provider mode + MiniMax key to config validation"
```

注意：此任务改了 `DeviceConfig` 结构体，`src/` 里所有构造/拷贝它的地方后续任务会跟进；本任务只保证 native 测试通过（native 不编 `src/`）。

---

### Task 5: command_parser 加 SET:PROVIDER / SET:MMKEY

**Files:**
- Modify: `lib/core/src/command_parser.h`
- Modify: `lib/core/src/command_parser.cpp`
- Test: `test/test_command_parser/main.cpp`（追加）

- [ ] **Step 1: command_parser.h 改 CommandType 与 Command**

enum 末尾追加两个值；Command 加两个字段；顶部 `#include "config_validate.h"`（用 ProviderMode）：

```cpp
#pragma once
#include <stdint.h>
#include "config_validate.h"

enum CommandType : uint8_t {
  CMD_UNKNOWN = 0,
  CMD_GET_CONFIG,
  CMD_SET_WIFI,
  CMD_SET_KEY,
  CMD_SET_INTERVAL,
  CMD_REFRESH,
  CMD_GET_USAGE,
  CMD_RESET_CONFIG,
  CMD_REBOOT,
  CMD_SET_PROVIDER,
  CMD_SET_MMKEY
};

struct Command {
  CommandType type;
  char ssid[64];
  char password[64];
  char key[128];
  char mmkey[128];
  long interval;
  uint8_t provider_mode; // ProviderMode，仅 CMD_SET_PROVIDER 有效
};

// 解析一行命令（不含换行）。合法返回 true 并填 *out，非法返回 false。
bool parse_command(const char* line, Command* out);
```

- [ ] **Step 2: command_parser.cpp 追加两个分支**

`parse_command` 里 out 初始化加 `out->mmkey[0] = '\0'; out->provider_mode = MODE_KIMI;`，然后在 `SET:WIFI:` 分支前加：

```cpp
  if (strncmp(line, "SET:PROVIDER:", 13) == 0) {
    const char* v = line + 13;
    if (strcmp(v, "kimi") == 0)    out->provider_mode = MODE_KIMI;
    else if (strcmp(v, "minimax") == 0) out->provider_mode = MODE_MINIMAX;
    else if (strcmp(v, "both") == 0)    out->provider_mode = MODE_BOTH;
    else return false;
    out->type = CMD_SET_PROVIDER;
    return true;
  }

  if (strncmp(line, "SET:MMKEY:", 10) == 0) {
    const char* key = line + 10;
    if (!*key) return false;
    copy_str(out->mmkey, sizeof(out->mmkey), key);
    out->type = CMD_SET_MMKEY;
    return true;
  }
```

- [ ] **Step 3: 追加测试（并注册）**

```cpp
void test_set_provider() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:PROVIDER:kimi", &c));
  TEST_ASSERT_EQUAL(CMD_SET_PROVIDER, c.type);
  TEST_ASSERT_EQUAL(MODE_KIMI, c.provider_mode);
  TEST_ASSERT_TRUE(parse_command("SET:PROVIDER:minimax", &c));
  TEST_ASSERT_EQUAL(MODE_MINIMAX, c.provider_mode);
  TEST_ASSERT_TRUE(parse_command("SET:PROVIDER:both", &c));
  TEST_ASSERT_EQUAL(MODE_BOTH, c.provider_mode);
  TEST_ASSERT_FALSE(parse_command("SET:PROVIDER:openai", &c));
  TEST_ASSERT_FALSE(parse_command("SET:PROVIDER:", &c));
}

void test_set_mmkey() {
  Command c;
  TEST_ASSERT_TRUE(parse_command("SET:MMKEY:mm-abcdef123456", &c));
  TEST_ASSERT_EQUAL(CMD_SET_MMKEY, c.type);
  TEST_ASSERT_EQUAL_STRING("mm-abcdef123456", c.mmkey);
  TEST_ASSERT_FALSE(parse_command("SET:MMKEY:", &c));
}
```

- [ ] **Step 4: 跑测试确认通过（旧 6 + 新 2）**

Run: `$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native -f test_command_parser`

- [ ] **Step 5: Commit**

```bash
git add lib/core/src/command_parser.* test/test_command_parser
git commit -m "feat(core): add SET:PROVIDER and SET:MMKEY commands"
```

---

### Task 6: config_store 存取 mode/mmkey（硬件层）

**Files:**
- Modify: `src/config_store.cpp`

- [ ] **Step 1: 改 load/save**

`config_store_load` 在 `p.getLong("interval", 60)` 后加：

```cpp
  String mmkey = p.getString("mmkey", "");
  uint8_t mode = p.getUChar("mode", MODE_KIMI); // 旧配置无此键 → 默认 kimi，无缝升级
```

并在填 `refresh_interval` 后加：

```cpp
  strncpy(cfg->minimax_key, mmkey.c_str(), sizeof(cfg->minimax_key) - 1);
  cfg->minimax_key[sizeof(cfg->minimax_key) - 1] = '\0';
  cfg->provider_mode = mode > MODE_BOTH ? MODE_KIMI : mode; // NVS 脏数据兜底
```

`config_store_save` 在 `p.putLong(...)` 后加：

```cpp
  p.putString("mmkey", cfg->minimax_key);
  p.putUChar("mode", cfg->provider_mode);
```

- [ ] **Step 2: 验证 esp32 编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`（此时 main.cpp 还没用新字段，结构体变大不影响编译）

- [ ] **Step 3: Commit**

```bash
git add src/config_store.cpp
git commit -m "feat(config): persist provider mode + MiniMax key in NVS"
```

---

### Task 7: root_ca.h 加 MiniMax 根证书 + kimi_net 泛化

**Files:**
- Modify: `src/root_ca.h`
- Modify: `src/kimi_net.h`
- Modify: `src/kimi_net.cpp`

MiniMax（www.minimaxi.com）证书链根为 USERTrust RSA Certification Authority（2038-01-18 到期，与 DigiCert G2 同级寿命；从 Windows 受信根存储导出）。

- [ ] **Step 1: root_ca.h 追加**

```cpp
// USERTrust RSA Certification Authority（www.minimaxi.com 证书链的根，2038-01-18 到期）
static const char* MINIMAX_ROOT_CA_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
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
Q+6IHdfGjjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----
)EOF";
```

- [ ] **Step 2: kimi_net.h 改**

顶部 `#include "provider.h"`；加新声明，保留旧声明（包装，注释说明）：

```cpp
// GET 对应 provider 的用量接口（URL 与 CA 按 provider 选择）。
// 对时有效 → setCACert(对应根证书) 严格校验；对时无效 → setInsecure() 降级。
NetResult net_fetch_usage(Provider p, const char* api_key, uint32_t timeout_ms);

// 兼容包装：等价 net_fetch_usage(PROVIDER_KIMI, ...)。旧调用点逐步迁移。
NetResult kimi_fetch_usage(const char* api_key, uint32_t timeout_ms);
```

- [ ] **Step 3: kimi_net.cpp 改**

- 删 `static const char* USAGE_URL`，改 `#include "provider.h"`
- `kimi_fetch_usage` 改名 `net_fetch_usage(Provider p, const char* api_key, uint32_t timeout_ms)`，体内两处变化：

```cpp
  WiFiClientSecure client;
  if (r.clock_valid) {
    client.setCACert(p == PROVIDER_MINIMAX ? MINIMAX_ROOT_CA_PEM : ROOT_CA_PEM);
  } else {
    client.setInsecure(); // 未对时降级，屏幕用 ! 标注
  }

  HTTPClient http;
  http.setTimeout(timeout_ms);
  if (!http.begin(client, provider_url(p))) {
```

- 文件末尾加包装：

```cpp
NetResult kimi_fetch_usage(const char* api_key, uint32_t timeout_ms) {
  return net_fetch_usage(PROVIDER_KIMI, api_key, timeout_ms);
}
```

- [ ] **Step 4: 验证 esp32 编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`（portal/main 仍走 kimi_fetch_usage 包装，行为不变）

- [ ] **Step 5: Commit**

```bash
git add src/root_ca.h src/kimi_net.*
git commit -m "feat(net): provider-aware fetch with MiniMax USERTrust root CA"
```

---

### Task 8: display 标题参数化 + 切换提示

**Files:**
- Modify: `src/display.h`
- Modify: `src/display.cpp`

- [ ] **Step 1: display.h 的 DisplayState 加两个字段**

```cpp
struct DisplayState {
  bool has_data;
  UsageData data;
  bool stale;
  long age_seconds;
  bool clock_valid;
  bool wifi_ok;
  const char* status_msg;
  bool key_invalid;
  const char* title;    // 大标题，如 "KIMI USAGE" / "MINIMAX USAGE"
  bool switch_hint;     // both 模式：状态栏提示可点击切换
};
```

- [ ] **Step 2: display.cpp 改**

1. `draw_title` 加参数：`static void draw_title(TFT_eSPI* tft, const char* title)`，`drawString(title, ...)`。
2. 静态页（connecting/portal_hint/invalid_key）统一传 `"USAGE MONITOR"`：

```cpp
static const char* STATIC_TITLE = "USAGE MONITOR";
```

各处 `draw_title(tft);` → `draw_title(tft, STATIC_TITLE);`
3. `display_draw_main` 里 `draw_title(tft);` → `draw_title(tft, st.title ? st.title : STATIC_TITLE);`
4. 状态栏组装处（现有 `snprintf(status, ...)` 两个分支之后、`setTextColor` 之前）加：

```cpp
  if (st.switch_hint) {
    size_t len = strlen(status);
    snprintf(status + len, sizeof(status) - len, "%s", len > 0 && status[len-1] != ' ' ? "  tap: switch" : "tap: switch");
  }
```

（`#include <string.h>` 如缺则补。）

- [ ] **Step 3: 验证 esp32 编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`（main.cpp 的 redraw 还没填新字段，`st.title` 为零初始化前的旧代码……**注意**：main.cpp 目前是栈上 `DisplayState st;` 未清零，新字段是野值。**本任务必须同时在 main.cpp 的 redraw() 开头加 `memset(&st, 0, sizeof(st));`**，或者等 Task 10 一起做——选择：本任务就加 memset，保证每步可编可跑。）

main.cpp `redraw()` 改：

```cpp
static void redraw() {
  DisplayState st;
  memset(&st, 0, sizeof(st));
  st.has_data = s_has_data;
  ...
```

- [ ] **Step 4: Commit**

```bash
git add src/display.* src/main.cpp
git commit -m "feat(display): parameterize title + optional tap-to-switch hint"
```

---

### Task 9: portal 表单加 provider 选择 + 逐家验证

**Files:**
- Modify: `src/portal.cpp`

- [ ] **Step 1: 表单 HTML 改**

`send_form` 里 R"HTML 段替换为（在 WiFi 密码后、Kimi Key 前插入 provider 选择与 MiniMax Key；Kimi Key 的 `required` 去掉，改由服务端按模式校验）：

```cpp
  html += R"HTML(<label>WiFi 密码（开放网络可留空）</label><input name="pass" type="password">
<label>用量服务商</label><select name="mode">
<option value="kimi" selected>仅 Kimi</option>
<option value="minimax">仅 MiniMax</option>
<option value="both">Kimi + MiniMax（点屏幕切换）</option>
</select>
<label>Kimi API Key（选了 Kimi 必填）</label><input name="key" placeholder="sk-...">
<label>MiniMax API Key（选了 MiniMax 必填）</label><input name="mmkey">
<label>刷新间隔（秒，30-3600，默认 60）</label><input name="interval" type="number" min="30" max="3600" value="60">
<button type="submit">保存并连接</button>
<div class="note">保存时会先验证 WiFi 和所选服务商的 API Key，全部通过才会写入设备。</div>
</form></body></html>)HTML";
```

页面标题（PAGE_HEAD 里两处 `Kimi 用量显示器配置`）改为 `用量显示器配置`。

- [ ] **Step 2: handle_save 改解析 + 校验文案**

`strncpy(c.api_key, ...)` 后加：

```cpp
  strncpy(c.minimax_key, s_server->arg("mmkey").c_str(), sizeof(c.minimax_key) - 1);
  c.minimax_key[sizeof(c.minimax_key) - 1] = '\0';
  String mode = s_server->arg("mode");
  c.provider_mode = mode == "minimax" ? MODE_MINIMAX : mode == "both" ? MODE_BOTH : MODE_KIMI;
```

校验失败文案改：

```cpp
    send_form(verr == CFG_ERR_BAD_INTERVAL ? "刷新间隔需在 30-3600 秒之间" : "请完整填写 WiFi 名称和所选服务商的 API Key");
```

- [ ] **Step 3: verify_config 改逐家验证**

整体替换（WiFi 只连一次，然后按模式逐家 fetch+parse）：

```cpp
// 验证一家：fetch + parse。返回空串为成功。
static String verify_one(Provider p, const char* key) {
  const char* name = p == PROVIDER_MINIMAX ? "MiniMax" : "Kimi";
  NetResult r = net_fetch_usage(p, key, 10000);
  if (r.status == NET_ERR_HTTP && (r.http_code == 401 || r.http_code == 403)) {
    return String(name) + " API Key 无效或已失效";
  }
  if (r.status == NET_ERR_HTTP) {
    return String(name) + " 服务器返回 HTTP " + r.http_code;
  }
  if (r.status != NET_OK) {
    return String("无法连接 ") + name + " 服务器，请检查网络";
  }
  UsageData d;
  if (provider_parse(p, r.body.c_str(), &d) != PARSE_OK) {
    return String(name) + " 返回数据异常，请稍后再试";
  }
  return "";
}

// 验证流程：连 WiFi → NTP → 按模式逐家 fetch。返回错误说明（给用户看的中文）。
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
  if (cfg.provider_mode != MODE_MINIMAX) {
    String err = verify_one(PROVIDER_KIMI, cfg.api_key);
    if (err.length()) return err;
  }
  if (cfg.provider_mode != MODE_KIMI) {
    String err = verify_one(PROVIDER_MINIMAX, cfg.minimax_key);
    if (err.length()) return err;
  }
  return ""; // 成功
}
```

include 区 `#include "usage_parser.h"` 改为 `#include "provider.h"`。

- [ ] **Step 4: 验证 esp32 编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028`
Expected: `[SUCCESS]`

- [ ] **Step 5: Commit**

```bash
git add src/portal.cpp
git commit -m "feat(portal): provider selection + per-provider pre-save verification"
```

---

### Task 10: main.cpp 双槽状态机 + 触摸切换

**Files:**
- Modify: `platformio.ini`（lib_deps 加触摸库）
- Modify: `src/main.cpp`

- [ ] **Step 1: platformio.ini 的 `[env:esp32-2432s028]` lib_deps 加一行**

```ini
    paulstoffregen/XPT2046_Touchscreen @ ^1.4
```

- [ ] **Step 2: main.cpp 顶部 include 与常量**

include 区加：

```cpp
#include <XPT2046_Touchscreen.h>
#include "provider.h"
```

常量区加：

```cpp
// CYD 触摸（XPT2046）走独立 HSPI
static const uint8_t TOUCH_CS = 33;
static const uint8_t TOUCH_IRQ = 36;
static const uint8_t TOUCH_SCK = 25;
static const uint8_t TOUCH_MISO = 39;
static const uint8_t TOUCH_MOSI = 32;
static const uint32_t TOUCH_DEBOUNCE_MS = 300;
```

- [ ] **Step 3: 全局状态改双槽**

把这几个全局：

```cpp
static bool s_has_data = false;
static UsageData s_data;
static uint32_t s_data_fetched_ms = 0;
static bool s_key_invalid = false;
static const char* s_status_msg = "";
static int s_api_fail_count = 0;
```

替换为：

```cpp
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
static XPT2046_Touchscreen s_touch(TOUCH_CS, TOUCH_IRQ);
```

- [ ] **Step 4: redraw / fetch_and_update 改按槽**

```cpp
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
```

- [ ] **Step 5: 触摸切换函数 + setup/loop 接线**

新增：

```cpp
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
  if (!s_touch.touched()) return;
  uint32_t now = millis();
  if (now - last_tap < TOUCH_DEBOUNCE_MS) return;
  last_tap = now;
  switch_provider();
}
```

`setup()` 里 `display_init(&tft);` 后加（SPI 对象必须是文件级 static，不能是 setup 的局部变量——`s_touch` 持有它的引用，局部变量出作用域即悬垂）：

```cpp
  s_touch_spi.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  s_touch.begin(s_touch_spi);
  s_touch.setRotation(0);
```

全局区（`s_touch` 声明旁）加：

```cpp
static SPIClass s_touch_spi(HSPI);
```

`loop()` 里 `check_boot_long_press();` 后加 `check_touch_switch();`。

- [ ] **Step 6: setup/hook 里激活 provider 初始化与配置变更**

`setup()` 里 `config_store_load(&s_cfg);` 后加：

```cpp
  s_active = s_cfg.provider_mode == MODE_MINIMAX ? PROVIDER_MINIMAX : PROVIDER_KIMI;
```

`hook_config_changed()` 替换为：

```cpp
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
```

`check_boot_long_press` 里的 `redraw()` 调用不变（已按槽工作）。`fetch_and_update` 旧定义删除（由 Step 4 的包装替代）。WiFi 掉线分支里的 `s_status_msg = "WiFi LOST";` 改为 `s_slots[s_active].status_msg = "WiFi LOST";`。

- [ ] **Step 7: 验证 esp32 编译 + native 全量回归**

```powershell
.venv\Scripts\pio.exe run -e esp32-2432s028   # [SUCCESS]
$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native   # 全部 PASS
```

- [ ] **Step 8: Commit**

```bash
git add platformio.ini src/main.cpp
git commit -m "feat(main): dual provider slots + tap-to-switch via touchscreen"
```

---

### Task 11: serial_console 新命令 + GET:CONFIG 扩展

**Files:**
- Modify: `src/serial_console.cpp`

- [ ] **Step 1: print_config 扩展**

```cpp
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
```

- [ ] **Step 2: execute 加两个 case（插在 CMD_SET_INTERVAL 之后）**

```cpp
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
```

- [ ] **Step 3: 验证 esp32 编译**

Run: `.venv\Scripts\pio.exe run -e esp32-2432s028` → `[SUCCESS]`

- [ ] **Step 4: Commit**

```bash
git add src/serial_console.cpp
git commit -m "feat(serial): add SET:PROVIDER/SET:MMKEY + extended GET:CONFIG"
```

---

### Task 12: 全量回归 + 文档 + repo 改名

**Files:**
- Modify: `README.md`, `README.zh-CN.md`, `AGENTS.md`, `CHANGELOG.md`, `pyproject.toml`

- [ ] **Step 1: 全量回归**

```powershell
$env:Path = "$env:USERPROFILE\scoop\apps\mingw-winlibs\current\bin;" + $env:Path; .venv\Scripts\pio.exe test -e native   # 全部 PASS（约 40 用例）
.venv\Scripts\pio.exe run -e esp32-2432s028   # [SUCCESS]
```

- [ ] **Step 2: 文档更新**

- `README.md` / `README.zh-CN.md`：
  - 项目名改 `esp32-cyd-llm-usage`，简介加 MiniMax 与点屏切换
  - Features 加：MiniMax provider、both 模式点屏切换
  - 串口表加两行：`SET:PROVIDER:<kimi|minimax|both>`（切换服务商模式）、`SET:MMKEY:<apikey>`（改 MiniMax Key）
  - 重新配置一节补 SET:PROVIDER/SET:MMKEY 示例
- `AGENTS.md`：串口表加上述两行；手工验证清单追加：
  ```
  - [ ] both 模式配网：两 key 分别失败/成功的回显正确
  - [ ] both 模式主界面：标题正确、点击切换、切换后 Fetching→数据
  - [ ] 单 minimax 模式：与 kimi 单模式行为一致
  - [ ] 旧配置升级：烧新固件后不丢配置、默认 kimi 模式
  - [ ] MiniMax 数字与桌面端一致
  ```
- `CHANGELOG.md`：顶部加 `## [Unreleased]` 段，Added 列：MiniMax provider（percent 映射、USERTrust RSA CA）、both 模式触摸屏切换、双 RAM 槽独立错误状态、串口 SET:PROVIDER/SET:MMKEY、GET:CONFIG 扩展
- `pyproject.toml`：`name = "esp32-cyd-llm-usage"`

- [ ] **Step 3: Commit 文档**

```bash
git add README.md README.zh-CN.md AGENTS.md CHANGELOG.md pyproject.toml
git commit -m "docs: rename to esp32-cyd-llm-usage, document MiniMax + tap switch"
```

- [ ] **Step 4: repo 改名 + About/topics**

```bash
gh repo rename esp32-cyd-llm-usage --yes
git remote set-url origin git@github.com:wx528/esp32-cyd-llm-usage.git
gh repo edit --description "Standalone LLM coding plan usage monitor (Kimi + MiniMax) on ESP32-2432S028 (Cheap Yellow Display) — WiFi-direct, captive-portal setup, tap to switch providers"
gh repo edit --add-topic minimax
```

改名后验证：`git fetch origin` 正常、`gh repo view` 显示新名。

- [ ] **Step 5: 硬件验证（人工）**

按 AGENTS.md 更新后的清单走一遍（烧录流程见 AGENTS.md：按住 BOOT → upload → 松 BOOT → RST）。

---

## 执行顺序与依赖

```
Task 1（ms 转换）→ Task 2（minimax parser）→ Task 3（provider 分发）→ Task 4（config 校验）→ Task 5（命令解析）
  以上纯逻辑 TDD，严格按序（后者 include 前者）
Task 6（NVS）→ Task 7（net 泛化）→ Task 8（display）→ Task 9（portal）→ Task 10（main 双槽+触摸）→ Task 11（串口）
  每个 Task 编过 esp32 即提交；Task 10 依赖 3/4/7/8，Task 11 依赖 5/6
Task 12（回归+文档+改名+硬件验证）
```

**硬件烧录验证点**：Task 10 完成后第一次烧录验证双槽与触摸切换；Task 12 走全部清单（含旧配置升级验证）。
