#pragma once
// Larmutvärdering med hysteres och händelselogg. Ren kod (ingen Arduino).
// Bitnumren (AlarmId) är desamma i JSON och i ESP-NOW-paketet.
#include <stdint.h>
#include "cmu.h"
#include "settings.h"
#include "current.h"

enum AlarmId : uint8_t {
  AL_CELL_HIGH = 0, AL_CELL_LOW, AL_DELTA, AL_TEMP_HIGH, AL_TEMP_LOW, AL_CMU_OFFLINE, AL_CAN_ERROR,
  AL_CUR_HIGH, AL_CUR_SAT, AL_COUNT
};
enum AlarmLevel : uint8_t { LV_NONE = 0, LV_WARN = 1, LV_ALARM = 2 };

static const uint8_t  ALARM_LOG_SIZE = 20;
static const uint32_t ALARM_BOOT_GRACE_MS = 5000;   // inget offline-larm direkt efter start

struct AlarmEvent {
  uint32_t upS;       // upptid i sekunder
  uint8_t  id;
  uint8_t  level;     // nivån larmet fick (vid släpp: nivån som släpptes)
  bool     active;    // true = kom/ändrades, false = släppte
  int32_t  value;
  char     loc[8];
};

struct AlarmState {
  uint8_t    level[AL_COUNT];
  int32_t    value[AL_COUNT];
  char       loc[AL_COUNT][8];
  AlarmEvent log[ALARM_LOG_SIZE];
  uint8_t    logHead;    // nästa skrivposition
  uint8_t    logCount;
};

void alarmsInit(AlarmState& a);
void alarmsEvaluate(AlarmState& a, const PackStats& st, const Settings& s, bool canErrorRecent, uint32_t nowMs,
                    const CurrentStatus* cur = nullptr);
uint32_t alarmBits(const AlarmState& a);   // bit id satt om nivån är LV_ALARM
uint32_t warnBits(const AlarmState& a);    // bit id satt om nivån är LV_WARN
const char* alarmName(uint8_t id);
const AlarmEvent* alarmLogAt(const AlarmState& a, uint8_t i);   // i = 0 är nyaste, nullptr om den inte finns
