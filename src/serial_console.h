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
