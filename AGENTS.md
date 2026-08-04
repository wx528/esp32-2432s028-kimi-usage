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
3. 看到 `Serial port COM7` 并打印 `Connecting.....` 时松开 BOOT
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
