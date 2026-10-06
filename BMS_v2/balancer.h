#pragma once
// Balanseringsbeslut för 0x3C3. Ren kod (ingen Arduino).
#include <stdint.h>
#include "cmu.h"
#include "settings.h"

enum BalState : uint8_t { BS_OFF = 0, BS_WAITING = 1, BS_ACTIVE = 2, BS_MANUAL = 3, BS_BLOCKED = 4 };

// Målspänning när balanseringen är av (0x0F3D, samma som den tidigare koden skickade).
static const uint16_t BAL_IDLE_TARGET_MV = 3901;
// Pågående Auto-balansering släpper först när högsta cell är så här mycket under startspänningen
// (mätbruset under laddning är ca ±10 mV).
static const uint16_t BAL_START_HYST_MV = 10;

struct BalanceDecision {
  BalState state;
  uint16_t targetmV;       // 0x3C3 byte 0-1
  uint8_t  mode;           // 0x3C3 byte 2: 0 = all bleed av, 1 = bleed för celler över målet
  bool     manualExpired;  // Manuell har gått ut – anroparen sätter balMode = BAL_OFF
  char     reason[96];
};

struct BalancerMemory {
  bool     active;         // Auto har startat (för hysteresen)
  uint8_t  lastMode;       // för att upptäcka lägesbyte
  uint32_t manualStartMs;
};

void balancerInit(BalancerMemory& m);
BalanceDecision balancerDecide(const PackStats& st, const Settings& s, BalancerMemory& mem, uint32_t nowMs);
const char* balStateName(BalState s);
