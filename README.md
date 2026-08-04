# esp32-2432s028-kimi-usage

在 ESP32-2432S028（Cheap Yellow Display / CYD）上独立显示 Kimi Coding Plan 用量。设备直连 WiFi 调用 Kimi API，无需电脑挂机。

*Standalone Kimi Coding Plan usage monitor on the ESP32-2432S028 (CYD): WiFi-direct, captive-portal config, no PC required.*

## 功能

- 周额度大圆环（已用百分比 + 数字 + 重置倒计时）
- 5 小时窗口进度条（已用/上限 + 倒计时）
- 用量配色：<70% 绿 / 70–90% 黄 / >90% 红
- 手机 captive portal 配网（中文页面），保存前先验证 WiFi + API Key，配置存 NVS
- 断网兜底：WiFi 断开数据转灰并标注年龄，自动重连；API 失败指数退避
- Key 失效（401/498）全屏提示；长按 BOOT 5 秒擦除配置重新配网
- 串口调试后门（读配置、改 key、立即刷新等）
- 纯逻辑与硬件分离：核心逻辑在 PC 上有 29 个单元测试

## 硬件

- ESP32-2432S028（CYD，240×320 ILI9341，GPIO0 BOOT 键）
- 引脚配置在 `platformio.ini` 的 build_flags 里（TFT_eSPI 免改 User_Setup.h）

## 编译与烧录

Python 环境用 [uv](https://docs.astral.sh/uv/) 管理：

```powershell
uv sync
.venv\Scripts\pio.exe run -e esp32-2432s028   # 编译
```

烧录（这块板子自动下载电路不工作）：

1. 按住 BOOT 不松
2. `.venv\Scripts\pio.exe run --target upload -e esp32-2432s028`
3. 看到 `Connecting.....` 松开 BOOT
4. `[SUCCESS]` 后按一下 RST

Linux/macOS 把 `.venv\Scripts\pio.exe` 换成 `.venv/bin/pio`，并在 `platformio.ini` 里把 `upload_port`/`monitor_port` 改成实际串口。

## 配网

1. 首次上电（或配置被擦除）进入 Setup 模式，屏幕显示热点信息
2. 手机连热点 `CYD-Kimi-Setup`（密码 `kimisetup`），系统自动弹出配置页（或手动打开 192.168.4.1）
3. 填 WiFi、Kimi API Key、刷新间隔（30–3600 秒），保存
4. 设备当场验证 WiFi + API Key，全部通过才写入并重启

## 串口后门（115200，`\n` 结尾）

| 命令 | 作用 |
|---|---|
| `GET:CONFIG` | 查看配置（key 遮蔽显示） |
| `SET:WIFI:<ssid>:<pass>` | 改 WiFi |
| `SET:KEY:<apikey>` | 改 API Key |
| `SET:INTERVAL:<30-3600>` | 改刷新间隔 |
| `REFRESH` | 立即拉取 |
| `GET:USAGE` | 提示看屏幕 |
| `RESET:CONFIG` | 擦除配置并重启 |
| `REBOOT` | 重启 |

附 Python 客户端：`python scripts/send_command.py "GET:CONFIG" --port COM7`

## 单元测试

纯逻辑（JSON 解析 / 格式化 / 配色 / 命令解析 / 配置校验 / 退避）可在 PC 上测试，无需硬件：

```powershell
.venv\Scripts\pio.exe test -e native   # 需要 g++（如 scoop install mingw-winlibs）
```

## 目录结构

- `lib/core/` — 纯逻辑（native 可测）
- `src/` — 硬件层（display / kimi_net / portal / config_store / serial_console / main）
- `test/` — 7 组 native 单测
- `scripts/` — Windows 编译修复 + 串口交互脚本
- `docs/` — 设计文档与实施计划

## 依赖

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — 屏幕驱动
- [ArduinoJson](https://arduinojson.org/) — JSON 解析
- ESP32 Arduino core 自带 WebServer / DNSServer / Preferences / WiFiClientSecure

## License

[MIT](LICENSE)
