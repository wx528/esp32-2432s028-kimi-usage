#include "portal.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "kimi_net.h"
#include "provider.h"

static WebServer* s_server = nullptr;
static DNSServer* s_dns = nullptr;
static volatile bool s_done = false;
static DeviceConfig s_cfg;

// 验证一家：fetch + parse。返回空串为成功。
static String verify_one(Provider p, const char* key) {
  const char* name = p == PROVIDER_MINIMAX ? "MiniMax" : "Kimi";
  NetResult r = net_fetch_usage(p, key, 10000);
  if (r.status == NET_ERR_HTTP && (r.http_code == 401 || r.http_code == 403)) {
    return String(name) + " API Key 无效或已失效";
  }
  if (r.status == NET_ERR_HTTP) {
    return String(name) + " 服务器返回 HTTP " + r.http_code;
  }
  if (r.status != NET_OK) {
    return String("无法连接 ") + name + " 服务器，请检查网络";
  }
  UsageData d;
  if (provider_parse(p, r.body.c_str(), &d) != PARSE_OK) {
    return String(name) + " 返回数据异常，请稍后再试";
  }
  return "";
}

// 验证流程：连 WiFi → NTP → 按模式逐家 fetch。返回错误说明（给用户看的中文）。
// 调用时已处于 WIFI_AP_STA 模式，这里直接 WiFi.begin 即可，AP 保持不断。
static String verify_config(const DeviceConfig& cfg) {
  WiFi.begin(cfg.ssid, cfg.password);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    return "无法连接 WiFi，请检查名称和密码";
  }
  net_time_begin();
  net_time_wait(5000); // 对时失败不阻塞，fetch 会降级
  if (cfg.provider_mode != MODE_MINIMAX) {
    String err = verify_one(PROVIDER_KIMI, cfg.api_key);
    if (err.length()) return err;
  }
  if (cfg.provider_mode != MODE_KIMI) {
    String err = verify_one(PROVIDER_MINIMAX, cfg.minimax_key);
    if (err.length()) return err;
  }
  return ""; // 成功
}

static const char* PAGE_HEAD = R"HTML(<!DOCTYPE html><html lang="zh-CN"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>用量显示器配置</title>
<style>
body{font-family:system-ui,sans-serif;max-width:420px;margin:24px auto;padding:0 16px;background:#f5f5f5;color:#222}
h1{font-size:20px}
label{display:block;margin:14px 0 4px;font-weight:600}
input,select{width:100%;padding:10px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box;font-size:15px}
button{width:100%;margin-top:20px;padding:12px;background:#0078d4;color:#fff;border:0;border-radius:6px;font-size:16px}
.err{background:#fde7e9;color:#a80000;padding:10px;border-radius:6px;margin-top:14px}
.note{color:#666;font-size:13px;margin-top:6px}
</style></head><body><h1>用量显示器配置</h1>)HTML";

// HTML 转义：& 必须最先替换。
static String html_escape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    switch (s[i]) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += s[i];
    }
  }
  return out;
}

// 扫描周边 WiFi，返回 <option> 列表。调用前需已处于 WIFI_AP_STA 模式。
static String scan_ssid_options() {
  int n = WiFi.scanNetworks();
  String opts;
  for (int i = 0; i < n; i++) {
    opts += "<option value=\"" + html_escape(WiFi.SSID(i)) + "\">";
  }
  WiFi.scanDelete();
  return opts;
}

static void send_form(const String& err) {
  String html = PAGE_HEAD;
  if (err.length()) html += "<div class='err'>" + err + "</div>";
  html += "<form method=\"POST\" action=\"/save\">\n";
  html += "<label>WiFi 名称（从列表选或手动输入）</label>\n";
  html += "<input name=\"ssid\" list=\"ssids\" required autocomplete=\"off\">\n";
  html += "<datalist id=\"ssids\">" + scan_ssid_options() + "</datalist>\n";
  html += R"HTML(<label>WiFi 密码（开放网络可留空）</label><input name="pass" type="password">
<label>用量服务商</label><select name="mode">
<option value="kimi" selected>仅 Kimi</option>
<option value="minimax">仅 MiniMax</option>
<option value="both">Kimi + MiniMax（点屏幕切换）</option>
</select>
<label>Kimi API Key（选了 Kimi 必填）</label><input name="key" placeholder="sk-...">
<label>MiniMax API Key（选了 MiniMax 必填）</label><input name="mmkey">
<label>刷新间隔（秒，30-3600，默认 60）</label><input name="interval" type="number" min="30" max="3600" value="60">
<button type="submit">保存并连接</button>
<div class="note">保存时会先验证 WiFi 和所选服务商的 API Key，全部通过才会写入设备。</div>
</form></body></html>)HTML";
  s_server->send(200, "text/html", html);
}

static void handle_root() { send_form(""); }

static void handle_save() {
  DeviceConfig c;
  strncpy(c.ssid, s_server->arg("ssid").c_str(), sizeof(c.ssid) - 1);
  c.ssid[sizeof(c.ssid) - 1] = '\0';
  strncpy(c.password, s_server->arg("pass").c_str(), sizeof(c.password) - 1);
  c.password[sizeof(c.password) - 1] = '\0';
  strncpy(c.api_key, s_server->arg("key").c_str(), sizeof(c.api_key) - 1);
  c.api_key[sizeof(c.api_key) - 1] = '\0';
  strncpy(c.minimax_key, s_server->arg("mmkey").c_str(), sizeof(c.minimax_key) - 1);
  c.minimax_key[sizeof(c.minimax_key) - 1] = '\0';
  String mode = s_server->arg("mode");
  c.provider_mode = mode == "minimax" ? MODE_MINIMAX : mode == "both" ? MODE_BOTH : MODE_KIMI;
  c.refresh_interval = s_server->arg("interval").toInt();
  if (c.refresh_interval <= 0) c.refresh_interval = 60;

  ConfigError verr = validate_config(&c);
  if (verr != CFG_OK) {
    send_form(verr == CFG_ERR_BAD_INTERVAL ? "刷新间隔需在 30-3600 秒之间" : "请完整填写 WiFi 名称和所选服务商的 API Key");
    return;
  }

  String err = verify_config(c);
  if (err.length()) { send_form(err); return; }

  s_cfg = c;
  s_done = true;
  s_server->send(200, "text/html",
    "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'></head>"
    "<body style='font-family:system-ui;max-width:420px;margin:40px auto;padding:0 16px'>"
    "<h2>配置成功</h2><p>设备即将重启并开始显示用量，本热点会自动关闭。</p></body></html>");
}

static void handle_captive() { //  captive portal 探测地址统一重定向到表单
  s_server->sendHeader("Location", "http://192.168.4.1/", true);
  s_server->send(302, "text/plain", "");
}

PortalResult portal_run(const char* ap_name, const char* ap_pass, uint32_t timeout_ms) {
  PortalResult result;
  result.submitted = false;
  s_done = false;

  WiFi.mode(WIFI_AP_STA); // AP + STA：AP 提供配置页，STA 用于扫描和保存前验证
  WiFi.softAP(ap_name, ap_pass);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  s_dns = new DNSServer();
  s_dns->start(53, "*", apIP); // 所有域名劫持到设备，触发系统弹窗

  s_server = new WebServer(80);
  s_server->on("/", HTTP_GET, handle_root);
  s_server->on("/save", HTTP_POST, handle_save);
  // 常见 captive portal 探测路径
  s_server->on("/generate_204", HTTP_GET, handle_captive);        // Android
  s_server->on("/hotspot-detect.html", HTTP_GET, handle_captive); // iOS
  s_server->on("/ncsi.txt", HTTP_GET, handle_captive);            // Windows
  s_server->onNotFound(handle_captive);
  s_server->begin();

  uint32_t start = millis();
  while (!s_done && millis() - start < timeout_ms) {
    s_dns->processNextRequest();
    s_server->handleClient();
    delay(2);
  }

  if (s_done) {
    result.submitted = true;
    result.cfg = s_cfg;
    delay(1500); // 给成功页一点时间送达
  }
  s_server->stop();
  s_dns->stop();
  delete s_server; s_server = nullptr;
  delete s_dns; s_dns = nullptr;
  WiFi.softAPdisconnect(true);
  return result;
}
