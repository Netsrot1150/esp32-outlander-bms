#include "history.h"
#include <math.h>
#include <string.h>

void histInit(HistRing& r, HistRec* storage, uint16_t cap) {
  r.buf = storage;
  r.cap = cap;
  r.head = 0;
  r.count = 0;
}

void histPush(HistRing& r, const HistRec& rec) {
  r.buf[r.head] = rec;
  r.head = (uint16_t)((r.head + 1) % r.cap);
  if (r.count < r.cap) r.count++;
}

const HistRec& histAt(const HistRing& r, uint16_t i) {
  const uint16_t start = (uint16_t)((r.head + r.cap - r.count) % r.cap);
  return r.buf[(start + i) % r.cap];
}

void histLoad(HistRing& r, const HistRec* recs, uint16_t n) {
  for (uint16_t i = 0; i < n; i++)
    if (!(recs[i].t & HIST_REL_FLAG)) histPush(r, recs[i]);
}

void histFixRelative(HistRing& r, uint32_t nowEpoch, uint32_t nowUptimeS) {
  for (uint16_t i = 0; i < r.count; i++) {   // giltiga poster ligger på index 0..count-1 (eller hela bufferten)
    HistRec& rec = r.buf[i];
    if (rec.t & HIST_REL_FLAG) {
      const uint32_t up = rec.t & ~HIST_REL_FLAG;
      rec.t = nowEpoch - (nowUptimeS - up);
    }
  }
}

uint32_t histStamp(bool clockKnown, uint32_t epoch, uint32_t uptimeS) {
  return clockKnown ? epoch : ((uptimeS & ~HIST_REL_FLAG) | HIST_REL_FLAG);
}

static int8_t clampI8(int v) { return (int8_t)(v < -127 ? -127 : (v > 127 ? 127 : v)); }

static int16_t toDA(double amps) {
  const long v = lround(amps * 10.0);
  return (int16_t)(v < -32767 ? -32767 : (v > 32767 ? 32767 : v));
}

static HistRec makeRec(uint32_t t, double packmV, double minmV, double maxmV, bool haveT, int minT, int maxT,
                       bool haveCur, double amps, double soc) {
  HistRec r;
  r.t = t;
  r.packdV = (uint16_t)lround(packmV / 100.0);
  r.minCellmV = (uint16_t)lround(minmV);
  r.maxCellmV = (uint16_t)lround(maxmV);
  r.minT = haveT ? clampI8(minT) : HIST_NO_TEMP;
  r.maxT = haveT ? clampI8(maxT) : HIST_NO_TEMP;
  r.currentdA = haveCur ? toDA(amps) : HIST_NO_CURRENT;
  r.socX10 = (uint16_t)lround(soc * 10.0);
  return r;
}

void accumReset(HistAccum& a) {
  memset(&a, 0, sizeof(a));
  a.minT = INT16_MAX;
  a.maxT = INT16_MIN;
}

void accumAdd(HistAccum& a, const PackStats& st, bool haveCurrent, float amps, float socPct) {
  if (!st.hasCells) return;
  a.n++;
  a.packmV += st.packmV;
  a.minmV += st.minCellmV;
  a.maxmV += st.maxCellmV;
  a.socPct += socPct;
  if (haveCurrent) {
    a.amps += amps;
    a.nCur++;
  }
  if (st.minTempC != TEMP_INVALID) {
    if (st.minTempC < a.minT) a.minT = st.minTempC;
    if (st.maxTempC > a.maxT) a.maxT = st.maxTempC;
    a.nTemp++;
  }
}

bool accumMake(const HistAccum& a, uint32_t t, HistRec& out) {
  if (a.n == 0) return false;
  out = makeRec(t, a.packmV / a.n, a.minmV / a.n, a.maxmV / a.n, a.nTemp > 0, a.minT, a.maxT, a.nCur > 0,
                a.nCur ? a.amps / a.nCur : 0.0, a.socPct / a.n);
  return true;
}

HistRec histNow(const PackStats& st, bool haveCurrent, float amps, float socPct, uint32_t t) {
  return makeRec(t, st.packmV, st.minCellmV, st.maxCellmV, st.minTempC != TEMP_INVALID, st.minTempC, st.maxTempC,
                 haveCurrent, amps, socPct);
}
