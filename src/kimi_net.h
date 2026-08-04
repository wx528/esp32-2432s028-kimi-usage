#pragma once
#include <Arduino.h>
#include "provider.h"

enum NetStatus : uint8_t {
  NET_OK = 0,
  NET_ERR_WIFI,      // WiFi 未连接
  NET_ERR_TLS,       // TLS 握手/校验失败
  NET_ERR_HTTP,      // 非 200（http_code 有值）
  NET_ERR_TIMEOUT,
  NET_ERR_BODY       // 读响应体失败
};

struct NetResult {
  NetStatus status;
  int http_code;          // NET_ERR_HTTP 时的状态码，否则 0
  bool clock_valid;       // 本次请求时 NTP 是否已对时（决定有无证书校验）
  String body;            // NET_OK 时的响应体
};

// 初始化 NTP（configTime），不阻塞。重复调用安全。
void net_time_begin();

// 阻塞等待对时，最多 timeout_ms。对时成功返回 true。
bool net_time_wait(uint32_t timeout_ms);

// 当前系统时间是否为有效 UTC（NTP 已对时）
bool net_time_valid();

// GET 对应 provider 的用量接口（URL 与 CA 按 provider 选择）。
// 对时有效 → setCACert(对应根证书) 严格校验；对时无效 → setInsecure() 降级。
NetResult net_fetch_usage(Provider p, const char* api_key, uint32_t timeout_ms);
