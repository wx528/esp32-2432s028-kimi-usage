#pragma once
#include "config_validate.h"

// NVS 命名空间 "cydkimi"。字段键：ssid / pass / key / interval / mmkey / mode。
bool config_store_load(DeviceConfig* cfg);        // 读取；无有效配置返回 false
bool config_store_save(const DeviceConfig* cfg);  // 整份写入
void config_store_clear();                        // 擦除并重启由调用方决定
bool config_store_is_configured();                // load + validate == CFG_OK
