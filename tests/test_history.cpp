#include "test_framework.h"
#include "history.h"
#include <cstring>

static HistRec rec(uint32_t t) { HistRec r; std::memset(&r, 0, sizeof(r)); r.t = t; return r; }

TEST(hist_ringbuffert) {
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  for (uint32_t i = 1; i <= 6; i++) histPush(r, rec(i));
  CHECK_EQ(r.count, 4);
  CHECK_EQ(histAt(r, 0).t, 3); CHECK_EQ(histAt(r, 3).t, 6);
}

TEST(hist_relativ_tid_rattas) {
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  histPush(r, rec(histStamp(false, 0, 100)));
  histPush(r, rec(histStamp(false, 0, 160)));
  histPush(r, rec(histStamp(true, 1700000300, 200)));
  histFixRelative(r, 1700000300, 200);
  CHECK_EQ(histAt(r, 0).t, 1700000200u);
  CHECK_EQ(histAt(r, 1).t, 1700000260u);
  CHECK_EQ(histAt(r, 2).t, 1700000300u);
}

TEST(hist_inlasning_hoppar_over_relativa) {
  const HistRec saved[3] = {rec(1700000000), rec(50 | HIST_REL_FLAG), rec(1700000060)};
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  histLoad(r, saved, 3);
  CHECK_EQ(r.count, 2);
  CHECK_EQ(histAt(r, 1).t, 1700000060u);
}

TEST(hist_minutmedel) {
  PackStats a; std::memset(&a, 0, sizeof(a));
  a.hasCells = true; a.packmV = 246000; a.minCellmV = 3800; a.maxCellmV = 3850; a.minTempC = 15; a.maxTempC = 17;
  PackStats b = a; b.packmV = 248000; b.minCellmV = 3810; b.maxCellmV = 3860; b.minTempC = 14; b.maxTempC = 18;
  HistAccum acc; accumReset(acc);
  HistRec out;
  CHECK(!accumMake(acc, 0, out));
  accumAdd(acc, a, true, -10.0f, 60.0f);
  accumAdd(acc, b, true, -20.0f, 59.0f);
  CHECK(accumMake(acc, 1700000000, out));
  CHECK_EQ(out.t, 1700000000u);
  CHECK_EQ(out.packdV, 2470); CHECK_EQ(out.minCellmV, 3805); CHECK_EQ(out.maxCellmV, 3855);
  CHECK_EQ(out.minT, 14); CHECK_EQ(out.maxT, 18);
  CHECK_EQ(out.currentdA, -150); CHECK_EQ(out.socX10, 595);
}

TEST(hist_utan_sensor_och_temp) {
  PackStats a; std::memset(&a, 0, sizeof(a));
  a.hasCells = true; a.packmV = 246000; a.minTempC = TEMP_INVALID; a.maxTempC = TEMP_INVALID;
  const HistRec r = histNow(a, false, 0, 50, 123);
  CHECK_EQ(r.currentdA, HIST_NO_CURRENT); CHECK_EQ(r.minT, HIST_NO_TEMP); CHECK_EQ(r.t, 123u);
  CHECK_EQ(sizeof(HistRec), 16);
}
