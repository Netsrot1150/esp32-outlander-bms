#pragma once
// Broadcast av BmsPacket via ESP-NOW till skärmenheten.
#include <stdint.h>
#include "bms_packet.h"

bool espnowBegin();                    // anropas efter netBegin()
void espnowSend(const BmsPacket& p);
uint32_t espnowSentCount();
