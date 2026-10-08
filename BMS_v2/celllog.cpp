#include "celllog.h"
#include <string.h>

void clInit(CellLogRing& r, CellLogRec* storage, uint16_t cap) {
  r.buf = storage;
  r.cap = cap;
  r.head = 0;
  r.count = 0;
  r.total = 0;
}

void clPush(CellLogRing& r, const CellLogRec& rec) {
  r.buf[r.head] = rec;
  r.head = (uint16_t)((r.head + 1) % r.cap);
  if (r.count < r.cap) r.count++;
  r.total++;
}

const CellLogRec& clAt(const CellLogRing& r, uint16_t i) {
  const uint16_t start = (uint16_t)((r.head + r.cap - r.count) % r.cap);
  return r.buf[(start + i) % r.cap];
}

void clLoad(CellLogRing& r, const CellLogRec* recs, uint16_t n) {
  for (uint16_t i = 0; i < n; i++)
    if (!(recs[i].t & HIST_REL_FLAG)) clPush(r, recs[i]);
}

void clFixRelative(CellLogRing& r, uint32_t nowEpoch, uint32_t nowUptimeS) {
  for (uint16_t i = 0; i < r.count; i++) {
    CellLogRec& rec = r.buf[i];
    if (rec.t & HIST_REL_FLAG) rec.t = nowEpoch - (nowUptimeS - (rec.t & ~HIST_REL_FLAG));
  }
}

void cellAccumReset(CellAccum& a) { memset(&a, 0, sizeof(a)); }

void cellAccumAdd(CellAccum& a, const PackState& ps, const PackStats& st, uint32_t cmuTimeoutMs, uint32_t nowMs) {
  for (uint8_t m = 0; m < st.moduleCount && m < CELLLOG_CELLS / CELLS_PER_CMU; m++) {
    const CmuData& c = ps.cmu[st.moduleIds[m]];
    if (!cmuOnline(c, cmuTimeoutMs, nowMs)) continue;
    for (uint8_t k = 0; k < c.cellCount; k++) {
      if (c.cellmV[k] == 0) continue;
      const uint8_t i = (uint8_t)(m * CELLS_PER_CMU + k);
      a.sum[i] += c.cellmV[k];
      a.n[i]++;
    }
  }
}

bool cellAccumMake(const CellAccum& a, uint32_t t, CellLogRec& out) {
  uint16_t avg[CELLLOG_CELLS];
  uint16_t base = 0xFFFF;
  uint8_t used = 0;
  for (uint8_t i = 0; i < CELLLOG_CELLS; i++) {
    avg[i] = a.n[i] ? (uint16_t)((a.sum[i] + a.n[i] / 2) / a.n[i]) : 0;
    if (!avg[i]) continue;
    if (avg[i] < base) base = avg[i];
    used = (uint8_t)(i + 1);
  }
  if (!used) return false;
  memset(&out, 0, sizeof(out));
  out.t = t;
  out.basemV = base;
  out.n = used;
  for (uint8_t i = 0; i < CELLLOG_CELLS; i++) {
    const uint16_t d = avg[i] ? (uint16_t)(avg[i] - base) : CELLLOG_MISSING;
    out.off[i] = avg[i] ? (uint8_t)(d > CELLLOG_MAX_OFF ? CELLLOG_MAX_OFF : d) : CELLLOG_MISSING;
  }
  return true;
}
