# 多 Provider 支持（Kimi + MiniMax）— 设计文档

日期：2026-08-05
项目：esp32-2432s028-kimi-usage（随本 feature 改名为 esp32-cyd-llm-usage）

## 目标

同一台 CYD 设备可配置为只看 Kimi、只看 MiniMax、或两家都看。双 provider（both）模式下点击触摸屏切换显示哪家；定时只拉取当前激活的 provider，切换时立即拉取新激活的一家。

## 背景：MiniMax API（已在本机桌面端项目验证）

- `GET https://www.minimaxi.com/v1/token_plan/remains`，头：`Authorization: Bearer <key>`、`Accept: application/json`
- 响应取 `model_remains[]` 中 `model_name=="general"` 的条目（其余如 video 忽略）
- 用量以**百分比**为准：`used = 100 - current_weekly_remaining_percent` / `100 - current_interval_remaining_percent`；`current_*_total_count`/`usage_count` 自 MiniMax 转 token 计量后恒为 0，不可用
- 重置时间为**毫秒 epoch**：周额度 `weekly_end_time`，5 小时窗口 `end_time`
- 错误：`base_resp.status_code != 0` 为 API 层错误（`status_msg` 有说明，如 "unauthorized"）；key 无效走 HTTP 401；无 Kimi `boosterWallet STATUS_DISABLED` 的等价物
- CA 根证书需实现时从 `www.minimaxi.com` 实际证书链提取（同 Kimi 的 DigiCert G2 流程）

与 Kimi 的差异：Kimi 是字符串绝对值 + ISO8601 重置；MiniMax 是百分比数字 + 毫秒 epoch。两者都映射到现有 `UsageData`（MiniMax 的 limit 恒 100）。

## 架构

### Provider 抽象（lib/core，纯逻辑）

新增 `provider.h`：

```cpp
enum Provider : uint8_t { PROVIDER_KIMI = 0, PROVIDER_MINIMAX };

const char* provider_name(Provider p);      // "KIMI" / "MINIMAX"，屏幕标题用
const char* provider_url(Provider p);       // endpoint，net 层用
ParseResult provider_parse(Provider p, const char* json, UsageData* out); // 分发
```

- `PROVIDER_KIMI` 分发到现有 `parse_usage_json`（不动）
- `PROVIDER_MINIMAX` 分发到新的 `parse_minimax_json`

新增 `minimax_parser.{h,cpp}`（纯逻辑，native 可测）：

- `base_resp` 存在且 `status_code != 0` → 新枚举 `PARSE_ERR_API`（追加到 `ParseResult` 末尾，向后兼容）
- 缺 `model_remains` / 无 `general` 条目 → `PARSE_ERR_MISSING`
- percent 字段缺失或越界（非 0-100）→ `PARSE_ERR_BAD_VALUE`
- 重置时间经 `uint32_t ms_epoch_to_sec(int64_t ms)` 转换（放在 `time_parse` 模块；<=0 → 0）；转换结果为 0 → `PARSE_ERR_BAD_VALUE`（与 Kimi reset 解析失败的处理一致）
- 成功填 `UsageData`：used = 100 - remaining_percent，limit = 100

### 配置 / NVS / 配网页 / 串口

- `DeviceConfig` 增加：`uint8_t provider_mode`（0=kimi / 1=minimax / 2=both）、`char minimax_key[128]`
- NVS 新增键：`mode`（uint8）、`mmkey`（string）。旧配置载入时 mode 默认 kimi、mmkey 默认空——**无缝升级**
- `validate_config` 按模式校验对应 key 必填；mode 非法 → 新枚举 `CFG_ERR_BAD_MODE`
- 配网页表单：provider 选择（Kimi / MiniMax / 两个都要，radio 或 select）+ MiniMax Key 输入框。不引入 JS：两个 key 框常显，服务端按模式校验必填。保存时**逐家验证**（连 WiFi 后按模式依次 fetch + parse），错误回显指明哪家失败（如"MiniMax API Key 无效或已失效"）
- 串口新增命令：
  - `SET:PROVIDER:<kimi|minimax|both>`
  - `SET:MMKEY:<key>`
  - `GET:CONFIG` 返回增加 `mode` 与遮蔽后的 `mmkey`

### 触摸 / 状态机 / 显示

- **触摸**：XPT2046 走独立 SPI（CLK=25 / MOSI=32 / MISO=39 / CS=33 / IRQ=36），用 TFT_eSPI 自带触摸支持（`getTouch`），platformio.ini 补 `-DTOUCH_CS=33` 等 flags。仅 both 模式响应点击；软件防抖 300ms
- **状态机**（RUNNING）：
  - 新增 `active_provider`；每 provider 一个 RAM 缓存槽（UsageData + fetched_ms + status_msg + key_invalid + has_data）
  - 定时只拉激活 provider；单 provider 模式行为与现状完全一致
  - 点击（both 模式）→ 切 `active_provider` → 立即重绘（有缓存则转灰 + 年龄，无缓存则 "Fetching..."）→ 立刻拉取新激活 provider
  - 两家的 status/key_invalid/退避计数互相独立，互不污染
- **显示**：
  - 标题随 active provider 变：`KIMI USAGE` / `MINIMAX USAGE`（`draw_title` 加 name 参数）
  - both 模式状态栏加 `tap: switch` 提示
  - 其余布局、配色、转灰逻辑不动
- BOOT 长按擦除、串口行为不变

### 网络层

- `kimi_fetch_usage` 泛化为按 provider 选 URL + CA 证书（`www.minimaxi.com` 的根证书 PEM 实现时提取，与 ROOT_CA_PEM 并列存放）
- 文件名保留 `kimi_net.{h,cpp}` 不改，减少 churn

## 错误处理

| 情况 | 表现 |
|---|---|
| MiniMax HTTP 401 | 该 provider 槽 key_invalid → 全屏错误页（仅当它是 active 时全屏） |
| MiniMax `base_resp.status_code != 0` | 该槽 `BAD RESPONSE`，保留下次数据转灰 |
| 切到无缓存的 provider | "Fetching..."，拉取回来即刷新 |
| 一家失败一家正常 | 失败家状态栏报错；切到正常家不受影响 |

单 provider 模式的全部错误行为与当前版本一致。

## 测试策略

native 新增测试（复用桌面端项目的两组真实 MiniMax 响应样本：旧 count 版 + 新 percent 版）：

- `minimax_parser`：percent 版正常解析、旧 count 版按 percent 解析、base_resp 非零 → PARSE_ERR_API、缺 general → PARSE_ERR_MISSING、percent 越界 → PARSE_ERR_BAD_VALUE
- `ms_epoch_to_sec`：正常转换、0/负数 → 0
- `provider` 分发：两家各自路由正确、name/url 正确
- `config_validate`：三种模式的 key 必填矩阵、非法 mode
- `command_parser`：`SET:PROVIDER` 三个取值 + 非法值、`SET:MMKEY`

硬件手工验证清单（追加到 AGENTS.md）：

- [ ] both 模式配网：两 key 验证分别失败/成功的回显
- [ ] both 模式主界面：标题正确、点击切换、切换后 Fetching→数据
- [ ] 单 minimax 模式：与 kimi 单模式行为一致
- [ ] 旧配置升级：烧新固件后不丢配置、默认 kimi 模式
- [ ] MiniMax 数字与桌面端一致

## 改名与文档

- `gh repo rename esp32-cyd-llm-usage`（旧链接 GitHub 自动重定向）
- README（英/中）改项目名与描述、补 MiniMax 与触摸切换说明
- AGENTS.md 更新命令与验证清单
- About/topics 更新，topics 加 `minimax`
- CHANGELOG 开 `Unreleased` 段落；发布后转 `0.2.0`
- `pyproject.toml` name 同步

## 不做（YAGNI）

- 不每周期拉两家（只拉激活 provider）
- 不配网页 JS 动态显隐
- 不做更多 provider（抽象已就位，新增是后续小事）
- 不做触摸的其他用途（翻页/菜单）
