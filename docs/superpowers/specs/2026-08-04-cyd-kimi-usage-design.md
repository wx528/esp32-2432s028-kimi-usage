# ESP32-2432S028 Kimi 用量显示器 — 设计文档

日期：2026-08-04

## 目标

用 ESP32-2432S028（Cheap Yellow Display / CYD）独立显示 Kimi Coding Plan 的剩余用量。设备连 WiFi 后自行调用 Kimi API，无需连接电脑。烧录一次即可，WiFi 与 API Key 通过手机配网页面录入并持久化，因此同一份固件可以直接分享给他人使用。

## 范围

第一版只做 Kimi 一家 provider。不做触屏交互、不做背光调光、不做额度耗尽提醒。刷新间隔可在配网页面配置。

## 架构

### 数据链路

```
Kimi API  --WiFi/HTTPS-->  ESP32  -->  TFT 屏幕
```

设备直接 HTTPS 调用 `https://api.kimi.com/coding/v1/usages`，在设备上解析 JSON、计算百分比与重置倒计时。

### 模块划分

每个模块单独一组文件，职责单一，可独立理解。

| 模块 | 职责 | 依赖 |
|---|---|---|
| `config_store` | NVS 读写 WiFi SSID/密码、API Key、刷新间隔；提供"配置是否完整"判断 | Preferences |
| `portal` | AP 模式 + DNS 劫持 + 中文配置表单网页；提交后验证、写 NVS、重启 | WebServer, DNSServer |
| `kimi_client` | HTTPS 取数据；解析为 `UsageData` 结构体 | WiFiClientSecure, ArduinoJson |
| `display` | 全部绘屏：圆环、进度条、状态栏、错误页；只接收数据，不关心数据来源 | TFT_eSPI |
| `serial_console` | 串口调试后门：读写配置、强制刷新、擦除配置 | — |
| `main.cpp` | 状态机与定时调度 | 以上全部 |

### 可测试性约束

为了让核心逻辑能在 PC 上跑单元测试，纯逻辑必须与硬件 I/O 分离：

- `kimi_client`：`fetch()` 负责网络取字符串；`parse(const char* json)` 是不依赖 Arduino 的纯函数
- `display`：「算颜色、算文字」与「画到屏上」分成两层
- `serial_console`：命令解析是纯函数，执行动作单独一层

### 状态机

```
BOOT ──配置缺失──> PORTAL ──提交成功──> 重启
  │                  └──10 分钟超时──> 重启
  └──配置完整──> CONNECTING ──成功──> RUNNING
                     └──超时──> PORTAL
```

`RUNNING` 状态下 WiFi 断开自动重连；连续 30 次重连失败退回 `PORTAL`。在 `CONNECTING` 与 `RUNNING` 状态下长按 BOOT 键（GPIO0）5 秒擦除配置并重启进入 `PORTAL`，屏幕给出倒数提示（`PORTAL` 状态本身已在配网，无需此操作）。

## HTTPS 与时间

打包 DigiCert Global Root G2 根证书（`api.kimi.com` 证书链的根，2038-01 到期，覆盖设备物理寿命），开机通过 NTP（`pool.ntp.org`、`ntp.aliyun.com`）对时后启用证书校验。

对时失败时降级为 `setInsecure()`（跳过证书校验），并在状态栏显示 `!` 警示符，数据照常显示。后台每 10 分钟重试对时，成功后恢复校验。这样做的理由是可用性优先于严格校验——ESP32 无 RTC 电池，系统时间不对会导致证书有效期校验失败，若因此完全不显示数据则失去设备意义。

## 配网门户

### 屏幕提示

`PORTAL` 状态下屏幕显示 AP 名称 `CYD-Kimi-Setup`、AP 密码、以及 `192.168.4.1`。屏幕受字库限制只显示英文与数字，这些内容全部符合。

### 网页表单

手机连上 AP 后，DNS 劫持将所有域名解析到 `192.168.4.1`，触发系统「需要登录网络」弹窗自动打开配置页。页面为中文（浏览器渲染，无字库成本）。

| 字段 | 说明 | 校验 |
|---|---|---|
| WiFi 名称 | 下拉列表（进入 PORTAL 时扫描周边 WiFi），也允许手动输入以支持隐藏 SSID | 非空 |
| WiFi 密码 | password 输入框 | 允许为空（开放网络） |
| Kimi API Key | 文本框 | 非空 |
| 刷新间隔 | 数字，默认 60 秒 | 30–3600 秒 |

### 保存前验证

点击保存后先当场验证，全部通过才写入 NVS 并重启：

1. 连接 WiFi
2. 调用一次 Kimi API

任一步失败则不落盘，在网页上回显具体原因（WiFi 密码错误 / API Key 无效 / 网络不通），便于非技术用户自行排查。

### 超时

`PORTAL` 状态 10 分钟无人提交则重启，避免热点长期开放。由于配置仍然缺失，重启后会再次进入 `PORTAL`（相当于重置一次门户会话），不会陷入无法配网的状态。

## 串口调试后门

波特率 115200，命令以 `\n` 结尾。所有 `SET:` 命令在任何状态下可用，写入后不自动重启，允许连续设置多项后再 `REBOOT`。

| 命令 | 作用 | 成功响应 |
|---|---|---|
| `GET:CONFIG` | 打印当前配置 | `OK:CONFIG:{...}` |
| `SET:WIFI:<ssid>:<pass>` | 写入 WiFi 凭据 | `OK:SET:WIFI` |
| `SET:KEY:<apikey>` | 写入 API Key | `OK:SET:KEY` |
| `SET:INTERVAL:<sec>` | 写入刷新间隔 | `OK:SET:INTERVAL` |
| `REFRESH` | 立即拉取一次数据 | `OK:REFRESH` |
| `GET:USAGE` | 打印最近一次数据 | `OK:USAGE:{...}` |
| `RESET:CONFIG` | 擦除 NVS 并重启 | `OK:RESET` |
| `REBOOT` | 重启 | `OK:REBOOT` |

错误响应沿用 `ERR:<原因>` 格式。API Key 在任何输出中只显示前后各 4 位，防止串口日志泄露。

## 显示设计

240 × 320 竖屏，布局自上而下：

1. 标题 `KIMI USAGE`
2. 周额度大圆环（直径约 110px），环内显示已用百分比大字与 `WEEKLY` 标签
3. 周额度数字 `1,360 / 2,000` 与重置倒计时
4. 分隔线
5. 5 小时窗口：`5H WINDOW` 标签 + 细横向进度条 + `60/200` + 倒计时
6. 底部状态栏：WiFi 状态、最后更新时间、数据年龄

### 绘制细节

- 圆环用 TFT_eSPI 的 `fillArc` 绘制，只重绘发生变化的扇形段
- 全部走局部重绘，只擦除改动区域，避免整屏刷新闪烁
- 配色按已用百分比：< 70% 绿，70–90% 黄，> 90% 红
- 数字带千位分隔符
- 倒计时英文格式：`resets in 3d` / `in 5h` / `in 42m` / `resetting`

## 错误处理

原则：屏幕始终反映真实状况，绝不显示过期数据而不加标注。

| 情况 | 屏幕表现 | 恢复行为 |
|---|---|---|
| WiFi 断开 | 状态栏 `WiFi LOST`，数据区转灰并标注数据年龄（`5m ago`） | 每 10 秒重连，连续 30 次失败进 PORTAL |
| API 超时 / 网络错误 | 状态栏 `API TIMEOUT`，数据转灰并标注年龄 | 下周期重试，指数退避至最长 5 分钟 |
| HTTP 401 / 498（Key 失效） | 全屏错误页 `INVALID API KEY`，提示长按 BOOT 重新配置 | 停止重试，等待重新配置 |
| JSON 字段缺失 | 状态栏 `BAD RESPONSE`，保留上次数据并转灰 | 下周期重试 |
| NTP 对时失败 | 状态栏加 `!`，降级为不校验证书，数据正常显示 | 每 10 分钟重试对时 |

数据年龄机制：只要当前显示的数据不是本周期新取的，就在旁边标注 `Xm ago` 并将数据区转为灰色。

## 测试策略

### 主机端单元测试

PlatformIO `test/` 目录 + `native` 环境，`pio test -e native` 在 PC 上运行，覆盖：

- JSON 解析为 `UsageData`（含字段缺失、类型错误等异常输入）
- 百分比计算与配色阈值判定
- 倒计时格式化
- 千位分隔符格式化
- 串口命令解析
- 配置校验（刷新间隔边界、必填项）

### 硬件端手工验证清单

写入 `AGENTS.md`：

- 首次上电进入配网、手机连接、表单提交、验证失败回显、验证成功后正常显示
- 错误场景：断开路由器、填入错误 API Key、断电重启后自动恢复
- 长按 BOOT 5 秒重置配置
- 圆环重绘无可见闪烁

## 依赖

| 依赖 | 用途 | 来源 |
|---|---|---|
| `bodmer/TFT_eSPI` | 屏幕驱动（沿用现有项目的引脚配置） | PlatformIO 库 |
| `bblanchon/ArduinoJson` | JSON 解析 | PlatformIO 库 |
| `WebServer` / `DNSServer` / `Preferences` / `WiFiClientSecure` | 配网门户、NVS、HTTPS | ESP32 Arduino core 自带 |

不使用 WiFiManager：该库自带一套 UI，定制中文页面比直接用 `WebServer` 更繁琐。

## 烧录注意

实操经验：这块板子自动下载电路不起作用，需先按住 BOOT，运行上传命令，见到 `Connecting.....` 后松开 BOOT，上传成功后按 RST 才能进入正常运行模式。此流程写入 `AGENTS.md`。

## 参考

- 同系列 CYD 硬件项目（TFT_eSPI 引脚配置、局部重绘思路、烧录流程）
- 桌面端 Kimi 用量工具的 API 字段解析与倒计时格式化逻辑
