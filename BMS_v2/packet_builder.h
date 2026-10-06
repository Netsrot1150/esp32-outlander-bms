#pragma once
// Bygger BmsPacket från BMS-läget. Ren kod (ingen Arduino).
#include "bms_packet.h"
#include "cmu.h"
#include "balancer.h"

void buildBmsPacket(BmsPacket& p, const PackState& ps, const PackStats& st, const BalanceDecision& d,
                    uint32_t alarmBits, uint32_t warnBits, int16_t tempOffset, uint32_t cmuTimeoutMs,
                    uint32_t nowMs, uint8_t seq, uint16_t socX10, int16_t currentdA, uint8_t socMode);
