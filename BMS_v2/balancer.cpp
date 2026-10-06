#include "balancer.h"
#include <stdio.h>
#include <string.h>

void balancerInit(BalancerMemory& m) {
  m.active = false;
  m.lastMode = 0xFF;
  m.manualStartMs = 0;
}

const char* balStateName(BalState s) {
  switch (s) {
    case BS_OFF: return "OFF";
    case BS_WAITING: return "WAITING";
    case BS_ACTIVE: return "ACTIVE";
    case BS_MANUAL: return "MANUAL";
    case BS_BLOCKED: return "BLOCKED";
  }
  return "?";
}

static BalanceDecision idle(BalState st) {
  BalanceDecision d;
  memset(&d, 0, sizeof(d));
  d.state = st;
  d.targetmV = BAL_IDLE_TARGET_MV;
  d.mode = 0;
  return d;
}

BalanceDecision balancerDecide(const PackStats& st, const Settings& s, BalancerMemory& mem, uint32_t nowMs) {
  if (s.balMode != mem.lastMode) {
    mem.active = false;
    mem.lastMode = s.balMode;
    if (s.balMode == BAL_MANUAL) mem.manualStartMs = nowMs;
  }

  if (s.balMode == BAL_OFF) {
    BalanceDecision d = idle(BS_OFF);
    snprintf(d.reason, sizeof(d.reason), "Balansering avstängd");
    return d;
  }

  const uint32_t manualElapsed = nowMs - mem.manualStartMs;
  const uint32_t manualLimit = (uint32_t)s.manualTimeoutMin * 60000UL;
  if (s.balMode == BAL_MANUAL && manualElapsed >= manualLimit) {
    BalanceDecision d = idle(BS_OFF);
    d.manualExpired = true;
    snprintf(d.reason, sizeof(d.reason), "Manuell avslutad efter %u min", (unsigned)s.manualTimeoutMin);
    return d;
  }

  // Spärrar som gäller både Auto och Manuell
  if (st.online < s.expectedCmus) {
    mem.active = false;
    BalanceDecision d = idle(BS_BLOCKED);
    snprintf(d.reason, sizeof(d.reason), "Blockerad: %u av %u CMU online", (unsigned)st.online, (unsigned)s.expectedCmus);
    return d;
  }
  if (!st.hasCells) {
    mem.active = false;
    BalanceDecision d = idle(BS_BLOCKED);
    snprintf(d.reason, sizeof(d.reason), "Blockerad: inga cellvärden");
    return d;
  }
  if (st.maxTempC != TEMP_INVALID && st.maxTempC > s.balMaxTempC) {
    mem.active = false;
    BalanceDecision d = idle(BS_BLOCKED);
    snprintf(d.reason, sizeof(d.reason), "Blockerad: %d °C > %d °C", (int)st.maxTempC, (int)s.balMaxTempC);
    return d;
  }

  if (s.balMode == BAL_MANUAL) {
    BalanceDecision d = idle(BS_MANUAL);
    d.mode = 1;
    d.targetmV = s.manualTargetmV;
    const unsigned left = (unsigned)((manualLimit - manualElapsed + 59999UL) / 60000UL);
    snprintf(d.reason, sizeof(d.reason), "Manuell: mål %u mV, %u min kvar", (unsigned)s.manualTargetmV, left);
    return d;
  }

  // Auto
  const uint16_t startLimit = mem.active ? (uint16_t)(s.balStartmV - BAL_START_HYST_MV) : s.balStartmV;
  if (st.maxCellmV < startLimit) {
    mem.active = false;
    BalanceDecision d = idle(BS_WAITING);
    snprintf(d.reason, sizeof(d.reason), "Väntar: högsta cell %u mV < %u mV", (unsigned)st.maxCellmV, (unsigned)startLimit);
    return d;
  }
  if (!mem.active && st.deltamV < s.balStartDeltamV) {
    BalanceDecision d = idle(BS_WAITING);
    snprintf(d.reason, sizeof(d.reason), "Väntar: delta %u mV < %u mV", (unsigned)st.deltamV, (unsigned)s.balStartDeltamV);
    return d;
  }
  if (mem.active && st.deltamV <= s.balStopDeltamV) {
    mem.active = false;
    BalanceDecision d = idle(BS_WAITING);
    snprintf(d.reason, sizeof(d.reason), "Klar: delta %u mV ≤ %u mV", (unsigned)st.deltamV, (unsigned)s.balStopDeltamV);
    return d;
  }

  mem.active = true;
  BalanceDecision d = idle(BS_ACTIVE);
  d.mode = 1;
  const uint32_t fromMin = (uint32_t)st.minCellmV + s.balTargetOffsetmV;
  d.targetmV = (uint16_t)(fromMin > s.balStartmV ? fromMin : s.balStartmV);
  snprintf(d.reason, sizeof(d.reason), "Balanserar: delta %u mV, mål %u mV", (unsigned)st.deltamV, (unsigned)d.targetmV);
  return d;
}
