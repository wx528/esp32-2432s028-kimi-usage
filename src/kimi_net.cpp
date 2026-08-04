#include "kimi_net.h"
#include "root_ca.h"
#include "provider.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

static bool s_ntp_started = false;

void net_time_begin() {
  if (s_ntp_started) return;
  configTime(0, 0, "pool.ntp.org", "ntp.aliyun.com", "time.windows.com");
  s_ntp_started = true;
}

bool net_time_valid() {
  time_t now = time(nullptr);
  return now >= 1767225600; // 2026-01-01 UTC，早于它视为未对时
}

bool net_time_wait(uint32_t timeout_ms) {
  net_time_begin();
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (net_time_valid()) return true;
    delay(200);
  }
  return net_time_valid();
}

NetResult net_fetch_usage(Provider p, const char* api_key, uint32_t timeout_ms) {
  NetResult r;
  r.status = NET_OK;
  r.http_code = 0;
  r.clock_valid = net_time_valid();
  r.body = "";

  if (WiFi.status() != WL_CONNECTED) {
    r.status = NET_ERR_WIFI;
    return r;
  }

  WiFiClientSecure client;
  if (r.clock_valid) {
    client.setCACert(p == PROVIDER_MINIMAX ? MINIMAX_ROOT_CA_PEM : ROOT_CA_PEM);
  } else {
    client.setInsecure(); // 未对时降级，屏幕用 ! 标注
  }

  HTTPClient http;
  http.setTimeout(timeout_ms);
  if (!http.begin(client, provider_url(p))) {
    r.status = NET_ERR_TLS;
    return r;
  }
  http.addHeader("Authorization", String("Bearer ") + api_key);
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (code < 0) {
    r.status = (code == HTTPC_ERROR_READ_TIMEOUT || code == HTTPC_ERROR_CONNECTION_LOST)
                 ? NET_ERR_TIMEOUT : NET_ERR_TLS;
    http.end();
    return r;
  }
  if (code != 200) {
    r.status = NET_ERR_HTTP;
    r.http_code = code;
    http.end();
    return r;
  }

  r.body = http.getString();
  http.end();
  if (r.body.length() == 0) r.status = NET_ERR_BODY;
  return r;
}

NetResult kimi_fetch_usage(const char* api_key, uint32_t timeout_ms) {
  return net_fetch_usage(PROVIDER_KIMI, api_key, timeout_ms);
}
