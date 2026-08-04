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
