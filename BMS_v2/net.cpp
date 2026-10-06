#include "net.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <string.h>

static const uint32_t STA_RETRY_MS = 60000;
static uint32_t s_lastAttemptMs = 0;
static bool s_staEnabled = false;

static void startSta(const Settings& s) {
  if (s.staStatic) {
    WiFi.config(IPAddress(s.staIp[0], s.staIp[1], s.staIp[2], s.staIp[3]),
                IPAddress(s.staGw[0], s.staGw[1], s.staGw[2], s.staGw[3]),
                IPAddress(s.staMask[0], s.staMask[1], s.staMask[2], s.staMask[3]),
                IPAddress(s.staGw[0], s.staGw[1], s.staGw[2], s.staGw[3]));
  }
  WiFi.begin(s.staSsid, s.staPass[0] ? s.staPass : nullptr);
  s_lastAttemptMs = millis();
}

void netBegin(const Settings& s) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(false);   // återanslutning sköts av netLoop så att det egna nätet inte störs
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(s.apSsid, s.apPass);
  s_staEnabled = s.staSsid[0] != 0;
  if (s_staEnabled) startSta(s);
  if (MDNS.begin("bms")) MDNS.addService("http", "tcp", 80);
}

void netLoop(const Settings& s, uint32_t nowMs) {
  if (!s_staEnabled || WiFi.status() == WL_CONNECTED) return;
  if (nowMs - s_lastAttemptMs < STA_RETRY_MS) return;
  if (WiFi.softAPgetStationNum() > 0) {   // sökningen byter kanal och stör anslutna klienter
    s_lastAttemptMs = nowMs;
    return;
  }
  WiFi.disconnect();
  startSta(s);
}

void netStatus(NetStatus& o) {
  memset(&o, 0, sizeof(o));
  o.staConnected = WiFi.status() == WL_CONNECTED;
  if (o.staConnected) {
    strncpy(o.staIp, WiFi.localIP().toString().c_str(), sizeof(o.staIp) - 1);
    o.rssi = (int8_t)WiFi.RSSI();
  }
  strncpy(o.apIp, WiFi.softAPIP().toString().c_str(), sizeof(o.apIp) - 1);
  o.apClients = WiFi.softAPgetStationNum();
  o.channel = (uint8_t)WiFi.channel();
}
