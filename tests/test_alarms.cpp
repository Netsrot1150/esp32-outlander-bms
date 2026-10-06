#include "test_framework.h"
#include "alarms.h"
#include <cstring>

static PackStats st(uint16_t minmV, uint16_t maxmV, int16_t minT = 20, int16_t maxT = 25, uint8_t online = 8) {
  PackStats s; std::memset(&s, 0, sizeof(s));
  s.moduleCount = 8; s.online = online; s.cellCount = 64; s.hasCells = true;
  s.minCellmV = minmV; s.maxCellmV = maxmV; s.deltamV = (uint16_t)(maxmV - minmV);
  s.minLoc = {2, 4}; s.maxLoc = {6, 1};
  s.minTempC = minT; s.maxTempC = maxT; s.minTempMod = 0; s.maxTempMod = 3;
  return s;
}
static Settings def() { Settings s; settingsDefaults(s); return s; }
static const uint32_t T = 10000;   // efter uppstartsfönstret

TEST(larm_inget_vid_normala_varden) {
  AlarmState a; alarmsInit(a);
  alarmsEvaluate(a, st(3900, 3920), def(), false, T);
  CHECK_EQ(alarmBits(a), 0); CHECK_EQ(warnBits(a), 0); CHECK_EQ(a.logCount, 0);
}

TEST(larm_cell_hog_med_hysteres) {
  AlarmState a; alarmsInit(a);
  const Settings s = def();
  auto ev = [&](uint16_t mx) { alarmsEvaluate(a, st(mx - 10, mx), s, false, T); return a.level[AL_CELL_HIGH]; };
  CHECK_EQ(ev(4100), LV_NONE);
  CHECK_EQ(ev(4150), LV_WARN);
  CHECK_EQ(ev(4200), LV_ALARM);
  CHECK_EQ(ev(4185), LV_ALARM);   // släpper först under 4180
  CHECK_EQ(ev(4175), LV_WARN);
  CHECK_EQ(ev(4140), LV_WARN);    // varningen släpper först under 4130
  CHECK_EQ(ev(4125), LV_NONE);
  CHECK_EQ(a.logCount, 4);
  const AlarmEvent* e = alarmLogAt(a, 0);
  CHECK(e != nullptr && !e->active && e->level == LV_WARN && e->id == AL_CELL_HIGH);
  CHECK(std::strcmp(alarmLogAt(a, 3)->loc, "M7C2") == 0);
  CHECK(alarmLogAt(a, 4) == nullptr);
}

TEST(larm_cell_lag) {
  AlarmState a; alarmsInit(a);
  alarmsEvaluate(a, st(3290, 3300), def(), false, T);
  CHECK_EQ(a.level[AL_CELL_LOW], LV_WARN);
  CHECK_EQ(a.value[AL_CELL_LOW], 3290);
  CHECK(std::strcmp(a.loc[AL_CELL_LOW], "M3C5") == 0);
  alarmsEvaluate(a, st(3000, 3010), def(), false, T);
  CHECK_EQ(a.level[AL_CELL_LOW], LV_ALARM);
  CHECK(alarmBits(a) & (1u << AL_CELL_LOW));
}

TEST(larm_delta) {
  AlarmState a; alarmsInit(a);
  alarmsEvaluate(a, st(3900, 3950), def(), false, T);
  CHECK_EQ(a.level[AL_DELTA], LV_WARN);
  alarmsEvaluate(a, st(3900, 4000), def(), false, T);
  CHECK_EQ(a.level[AL_DELTA], LV_ALARM);
}

TEST(larm_temperatur) {
  AlarmState a; alarmsInit(a);
  const Settings s = def();
  alarmsEvaluate(a, st(3900, 3910, 20, 46), s, false, T);
  CHECK_EQ(a.level[AL_TEMP_HIGH], LV_WARN);
  CHECK(std::strcmp(a.loc[AL_TEMP_HIGH], "M4") == 0);
  alarmsEvaluate(a, st(3900, 3910, 20, 55), s, false, T);
  CHECK_EQ(a.level[AL_TEMP_HIGH], LV_ALARM);
  alarmsEvaluate(a, st(3900, 3910, -1, 20), s, false, T);
  CHECK_EQ(a.level[AL_TEMP_HIGH], LV_NONE);
  CHECK_EQ(a.level[AL_TEMP_LOW], LV_WARN);
  alarmsEvaluate(a, st(3900, 3910, 1, 20), s, false, T);
  CHECK_EQ(a.level[AL_TEMP_LOW], LV_WARN);   // släpper först vid 2 °C
  alarmsEvaluate(a, st(3900, 3910, 2, 20), s, false, T);
  CHECK_EQ(a.level[AL_TEMP_LOW], LV_NONE);
  alarmsEvaluate(a, st(3900, 3910, TEMP_INVALID, TEMP_INVALID), s, false, T);
  CHECK_EQ(a.level[AL_TEMP_HIGH], LV_NONE); CHECK_EQ(a.level[AL_TEMP_LOW], LV_NONE);
}

TEST(larm_cmu_offline_efter_uppstart) {
  AlarmState a; alarmsInit(a);
  const Settings s = def();
  alarmsEvaluate(a, st(3900, 3910, 20, 25, 7), s, false, 1000);
  CHECK_EQ(a.level[AL_CMU_OFFLINE], LV_NONE);   // uppstartsfönster
  alarmsEvaluate(a, st(3900, 3910, 20, 25, 7), s, false, 6000);
  CHECK_EQ(a.level[AL_CMU_OFFLINE], LV_ALARM);
  CHECK_EQ(a.value[AL_CMU_OFFLINE], 1);
  alarmsEvaluate(a, st(3900, 3910, 20, 25, 8), s, false, 7000);
  CHECK_EQ(a.level[AL_CMU_OFFLINE], LV_NONE);
}

TEST(larm_can_fel_ar_varning) {
  AlarmState a; alarmsInit(a);
  alarmsEvaluate(a, st(3900, 3910), def(), true, T);
  CHECK(warnBits(a) & (1u << AL_CAN_ERROR));
  CHECK_EQ(alarmBits(a), 0);
}

TEST(larm_utan_celldata_bara_offline) {
  AlarmState a; alarmsInit(a);
  PackStats s; std::memset(&s, 0, sizeof(s));
  s.moduleCount = 8; s.minTempC = TEMP_INVALID; s.maxTempC = TEMP_INVALID;
  alarmsEvaluate(a, s, def(), false, T);
  CHECK_EQ(a.level[AL_CELL_HIGH], LV_NONE); CHECK_EQ(a.level[AL_CELL_LOW], LV_NONE); CHECK_EQ(a.level[AL_DELTA], LV_NONE);
  CHECK_EQ(a.level[AL_CMU_OFFLINE], LV_ALARM);
  CHECK_EQ(a.value[AL_CMU_OFFLINE], 8);
}

TEST(larm_logg_ringbuffert) {
  AlarmState a; alarmsInit(a);
  for (int i = 0; i < 25; i++) alarmsEvaluate(a, st(3900, 3910), def(), i % 2 == 0, T + i);
  CHECK_EQ(a.logCount, ALARM_LOG_SIZE);
  CHECK(alarmLogAt(a, 0)->active);          // senaste (i = 24) satte varningen
  CHECK(!alarmLogAt(a, 1)->active);
  CHECK(alarmLogAt(a, 20) == nullptr);
  CHECK(std::strcmp(alarmName(AL_CAN_ERROR), "CAN-fel") == 0);
}

TEST(larm_hog_urladdningsstrom) {
  AlarmState a; alarmsInit(a); const Settings s = def();
  CurrentStatus c = {true, -61.0f, false};
  alarmsEvaluate(a, st(3900, 3910), s, false, T, &c);
  CHECK_EQ(a.level[AL_CUR_HIGH], LV_WARN); CHECK_EQ(a.value[AL_CUR_HIGH], 61);
  c.amps = -70.0f; c.saturated = true;
  alarmsEvaluate(a, st(3900, 3910), s, false, T, &c);
  CHECK_EQ(a.level[AL_CUR_HIGH], LV_ALARM); CHECK_EQ(a.level[AL_CUR_SAT], LV_WARN);
  c.amps = 30.0f; c.saturated = false;   // laddning larmar inte
  alarmsEvaluate(a, st(3900, 3910), s, false, T, &c);
  CHECK_EQ(a.level[AL_CUR_HIGH], LV_NONE); CHECK_EQ(a.level[AL_CUR_SAT], LV_NONE);
  CHECK(std::strcmp(alarmName(AL_CUR_HIGH), "Hög ström") == 0);
  CHECK(std::strcmp(alarmName(AL_CUR_SAT), "Strömsensor mättad") == 0);
}
