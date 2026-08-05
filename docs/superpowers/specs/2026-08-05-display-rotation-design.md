# 显示方向旋转（4 方向 + NVS 持久化）— 设计文档

日期：2026-08-05
项目：esp32-cyd-llm-usage

## 目标

支持 0°/90°/180°/270° 四个显示方向。运行中短按 BOOT 键循环切换，立即生效并写入 NVS，下次开机保持。

## 交互

- **BOOT 短按**（松开时按住时长 < 500ms）：方向 +1（0→1→2→3→0 循环），立即 `setRotation` + 全屏重绘 + 写 NVS
- **BOOT 长按 ≥ 500ms**：照旧进入"擦除配置"倒数提示，≥5 秒擦除。短按与长按互不干扰
- 0°/180° 为竖屏 240×320（现有布局），90°/270° 为横屏 320×240（新布局）

## 持久化

- `DeviceConfig` 加 `uint8_t rotation`（取值 0-3，与 TFT_eSPI `setRotation` 参数一致）
- NVS 键 `rot`（uint8），默认 0；load 时 `> 3 → 0` 兜底（与 provider_mode 同款脏数据防护）
- 旧配置无 `rot` 键 → 默认 0，无感升级
- `validate_config` 不校验 rotation（load 已兜底，不配网表单不涉及）

## 显示改造（display 层内置双布局表）

display.cpp 以 static 存当前方向；竖屏/横屏各一套坐标常量，绘屏函数按方向选表。

### 竖屏（rotation 0/2）

现有布局一个像素不动（标题 y=18、环心 (120,119) R75 W14、数字 y=208、倒计时 y=230、分隔线 y=248、5H y=258、条 y=276、窗口行 y=296、状态栏 (8,310)）。180° 由 `setRotation(2)` 硬件翻转，零额外代码。

### 横屏（rotation 1/3，320×240）左环右栏

| 元素 | 坐标 |
|---|---|
| 标题（如 `KIMI USAGE`，font4，cyan，居中） | 通宽居中，y=14 |
| 大圆环 + 环内百分比（font4）+ `WEEKLY`（font2） | 环心 (78,130)，R=60，W=12 |
| 周数字 `1,360 / 2,000`（font2，居中） | x 中心 225，y=95 |
| 周倒计时（font2，灰，居中） | x 中心 225，y=120 |
| 分隔线 | x=150..300，y=140 |
| `5H WINDOW`（font2，黄，左对齐） | x=150，y=152 |
| 细进度条 | x=150，宽 150，y=168，高 12 |
| 窗口行 `60/200  resets in 5h`（font2，灰，居中） | x 中心 225，y=192 |
| 状态栏（font2，ML_DATUM） | x=8，y=226，通宽 |

转灰、配色、`tap: switch` 提示等逻辑与竖屏一致，仅坐标不同。

### 静态页（配网提示 / Connecting / INVALID API KEY）

从硬编码 (120, y) 改为按 `tft->width()/height()` 相对居中，四方向均正常显示。文字内容不变。

## 触摸

`touched()` 不取坐标，方向切换对触摸无影响；切换方向时同步 `s_touch.setRotation(rotation)` 保持库状态一致（防御性，当前无坐标使用者）。

## main.cpp 改动

- 全局 `s_rotation`（uint8_t）：setup 里先 `config_store_load(&s_cfg)` 取 rotation（未配置时用 0），再 `display_init(&tft, s_rotation)`
- `check_boot_long_press` 松开分支：`held < 500ms` → `cycle_rotation()`（s_rotation=rotation_next → config_store 保存 → `tft.setRotation` + `s_touch.setRotation` → `redraw()`）；≥500ms 照旧 redraw 取消倒数
- `display_init` 签名加 rotation 参数

## 纯逻辑（native 可测）

- `display_model` 加 `uint8_t rotation_next(uint8_t r)`：`(r + 1) % 4`
- 测试：0→1、1→2、2→3、3→0、越界输入（如 7）→ 0

## 测试策略

- native：`rotation_next` 循环与越界
- 硬件手工验证清单（追加到 AGENTS.md）：
  - [ ] 短按 BOOT：方向循环 0→90→180→270→0，每次立即重绘
  - [ ] 横屏布局各元素位置正常（左环右栏、无重叠无越界）
  - [ ] 切换方向后断电重启：保持新方向
  - [ ] 旧配置升级：默认 0° 竖屏，行为不变
  - [ ] 短按不误触擦除；长按 5 秒擦除功能不变

## 不做（YAGNI）

- 不做串口 `SET:ROTATION`（BOOT 短按已覆盖日常需求）
- 不做配网页方向选择
- 不做触摸坐标映射（无坐标级触摸交互）
- 不做横屏下的布局微调动画
