#pragma once
// Eget WiFi (alltid på) + hemnät när det finns. mDNS: bms.local
#include <stdint.h>
#include "settings.h"

struct NetStatus {
  bool    staConnected;
  char    staIp[16];
  int8_t  rssi;
  uint8_t apClients;
  uint8_t channel;
  char    apIp[16];
};

void netBegin(const Settings& s);
void netLoop(const Settings& s, uint32_t nowMs);   // en gång per sekund
void netStatus(NetStatus& out);
