#include "alarms.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Ny nivå för ett "högt" värde. Höjning sker direkt vid tröskeln, sänkning först när värdet
// gått tillbaka mer än hyst under tröskeln för den nuvarande nivån.
static uint8_t levelHigh(uint8_t cur, int32_t v, int32_t warn, int32_t alarm, int32_t hyst) {
  const uint8_t raw = v >= alarm ? LV_ALARM : (v >= warn ? LV_WARN : LV_NONE);
  if (raw >= cur) return raw;
  if (cur == LV_ALARM && v > alarm - hyst) return LV_ALARM;
  if (v > warn - hyst) return LV_WARN;
  return raw;
}

static uint8_t levelLow(uint8_t cur, int32_t v, int32_t warn, int32_t alarm, int32_t hyst) {
  return levelHigh(cur, -v, -warn, -alarm, hyst);
}

void alarmsInit(AlarmState& a) { memset(&a, 0, sizeof(a)); }

const char* alarmName(uint8_t id) {
  switch (id) {
    case AL_CELL_HIGH: return "Cell hög";
    case AL_CELL_LOW: return "Cell låg";
    case AL_DELTA: return "Delta";
    case AL_TEMP_HIGH: return "Temp hög";
    case AL_TEMP_LOW: return "Temp låg";
    case AL_CMU_OFFLINE: return "CMU offline";
    case AL_CAN_ERROR: return "CAN-fel";
    case AL_CUR_HIGH: return "Hög ström";
    case AL_CUR_SAT: return "Strömsensor mättad";
  }
  return "?";
}

static void update(AlarmState& a, uint8_t id, uint8_t lvl, int32_t value, const char* loc, uint32_t nowMs) {
  const uint8_t old = a.level[id];
  if (lvl != LV_NONE) {
    a.value[id] = value;
    strncpy(a.loc[id], loc, sizeof(a.loc[id]) - 1);
    a.loc[id][sizeof(a.loc[id]) - 1] = 0;
  }
  if (lvl == old) return;
  a.level[id] = lvl;
  AlarmEvent& e = a.log[a.logHead];
  e.upS = nowMs / 1000;
  e.id = id;
  e.active = lvl != LV_NONE;
  e.level = e.active ? lvl : old;
  e.value = value;
  strncpy(e.loc, loc, sizeof(e.loc) - 1);
  e.loc[sizeof(e.loc) - 1] = 0;
  a.logHead = (uint8_t)((a.logHead + 1) % ALARM_LOG_SIZE);
  if (a.logCount < ALARM_LOG_SIZE) a.logCount++;
}

static void cellLoc(char* out, CellLoc l) {
  snprintf(out, 8, "M%uC%u", (unsigned)(l.module + 1) % 100, (unsigned)(l.cell + 1) % 10);
}

void alarmsEvaluate(AlarmState& a, const PackStats& st, const Settings& s, bool canErrorRecent, uint32_t nowMs,
                    const CurrentStatus* cur) {
  char loc[8];
  if (st.hasCells) {
    cellLoc(loc, st.maxLoc);
    update(a, AL_CELL_HIGH, levelHigh(a.level[AL_CELL_HIGH], st.maxCellmV, s.cellHighWarnmV, s.cellHighAlarmmV, 20),
           st.maxCellmV, loc, nowMs);
    cellLoc(loc, st.minLoc);
    update(a, AL_CELL_LOW, levelLow(a.level[AL_CELL_LOW], st.minCellmV, s.cellLowWarnmV, s.cellLowAlarmmV, 20),
           st.minCellmV, loc, nowMs);
    update(a, AL_DELTA, levelHigh(a.level[AL_DELTA], st.deltamV, s.deltaWarnmV, s.deltaAlarmmV, 10),
           st.deltamV, "", nowMs);
  } else {
    update(a, AL_CELL_HIGH, LV_NONE, 0, "", nowMs);
    update(a, AL_CELL_LOW, LV_NONE, 0, "", nowMs);
    update(a, AL_DELTA, LV_NONE, 0, "", nowMs);
  }

  if (st.maxTempC != TEMP_INVALID) {
    snprintf(loc, sizeof(loc), "M%u", (unsigned)(st.maxTempMod + 1) % 100);
    update(a, AL_TEMP_HIGH, levelHigh(a.level[AL_TEMP_HIGH], st.maxTempC, s.tempHighWarnC, s.tempHighAlarmC, 2),
           st.maxTempC, loc, nowMs);
    snprintf(loc, sizeof(loc), "M%u", (unsigned)(st.minTempMod + 1) % 100);
    update(a, AL_TEMP_LOW, levelLow(a.level[AL_TEMP_LOW], st.minTempC, s.tempLowWarnC, -1000, 2),
           st.minTempC, loc, nowMs);
  } else {
    update(a, AL_TEMP_HIGH, LV_NONE, 0, "", nowMs);
    update(a, AL_TEMP_LOW, LV_NONE, 0, "", nowMs);
  }

  uint8_t off = LV_NONE;
  int32_t missing = 0;
  if (nowMs >= ALARM_BOOT_GRACE_MS && st.online < s.expectedCmus) {
    off = LV_ALARM;
    missing = s.expectedCmus - st.online;
  }
  update(a, AL_CMU_OFFLINE, off, missing, "", nowMs);
  update(a, AL_CAN_ERROR, canErrorRecent ? LV_WARN : LV_NONE, 0, "", nowMs);

  if (cur && cur->enabled) {
    const int32_t outA10 = (int32_t)lroundf(-cur->amps * 10.0f);   // urladdning som positivt tal
    update(a, AL_CUR_HIGH, levelHigh(a.level[AL_CUR_HIGH], outA10, s.curWarnA * 10, s.curAlarmA * 10, 20),
           outA10 / 10, "", nowMs);
    update(a, AL_CUR_SAT, cur->saturated ? LV_WARN : LV_NONE, 0, "", nowMs);
  } else {
    update(a, AL_CUR_HIGH, LV_NONE, 0, "", nowMs);
    update(a, AL_CUR_SAT, LV_NONE, 0, "", nowMs);
  }
}

uint32_t alarmBits(const AlarmState& a) {
  uint32_t b = 0;
  for (uint8_t i = 0; i < AL_COUNT; i++) if (a.level[i] == LV_ALARM) b |= 1u << i;
  return b;
}

uint32_t warnBits(const AlarmState& a) {
  uint32_t b = 0;
  for (uint8_t i = 0; i < AL_COUNT; i++) if (a.level[i] == LV_WARN) b |= 1u << i;
  return b;
}

const AlarmEvent* alarmLogAt(const AlarmState& a, uint8_t i) {
  if (i >= a.logCount) return nullptr;
  return &a.log[(a.logHead + ALARM_LOG_SIZE - 1 - i) % ALARM_LOG_SIZE];
}
