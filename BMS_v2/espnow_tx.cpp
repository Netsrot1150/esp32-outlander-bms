#include "espnow_tx.h"
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool s_ok = false;
static uint32_t s_sent = 0;

bool espnowBegin() {
  if (esp_now_init() != ESP_OK) return false;
  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 0;          // aktuell kanal
  peer.ifidx = WIFI_IF_AP;   // eget nät är alltid på
  peer.encrypt = false;
  s_ok = esp_now_add_peer(&peer) == ESP_OK;
  return s_ok;
}

void espnowSend(const BmsPacket& p) {
  if (!s_ok) return;
  if (esp_now_send(BROADCAST, (const uint8_t*)&p, sizeof(p)) == ESP_OK) s_sent++;
}

uint32_t espnowSentCount() { return s_sent; }
