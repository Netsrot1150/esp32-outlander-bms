#include "cmu.h"
#include <string.h>

static uint16_t be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

// Sparar 4 cellspänningar. Ogiltiga värden ignoreras (föregående värde behålls).
static bool storeCells(CmuData& c, const uint8_t* d, int first) {
  bool anyValid = false;
  for (int i = 0; i < 4; i++) {
    const uint16_t v = be16(d + 2 * i);
    if (v >= CELL_VALID_MIN_MV && v <= CELL_VALID_MAX_MV) {
      const int k = first + i;
      const uint32_t target = (uint32_t)v << 16;
      c.cellRawmV[k] = v;
      if (c.cellmV[k] == 0) c.cellQ[k] = target;   // första värdet tas direkt
      else c.cellQ[k] = (uint32_t)((int32_t)c.cellQ[k] + ((int32_t)(target - c.cellQ[k])) / CELL_EMA_N);
      c.cellmV[k] = (uint16_t)((c.cellQ[k] + 0x8000u) >> 16);
      anyValid = true;
    } else {
      c.invalidCells++;
    }
  }
  return anyValid;
}

void packInit(PackState& ps) {
  memset(&ps, 0, sizeof(ps));
  for (int i = 0; i <= MAX_CMU_ID; i++) memset(ps.cmu[i].tempRaw, 0xFF, sizeof(ps.cmu[i].tempRaw));
}

bool decodeFrame(PackState& ps, uint32_t id, const uint8_t* d, uint8_t len, uint32_t nowMs) {
  if (id > 0x7FF || (id & 0xF00) != 0x600 || len != 8) return false;
  const uint8_t cmuId = (id >> 4) & 0x0F;
  const uint8_t sub = id & 0x0F;
  if (cmuId < 1 || cmuId > MAX_CMU_ID || sub < 1 || sub > CMU_SUBS) return false;

  CmuData& c = ps.cmu[cmuId];
  c.seen = true;
  c.lastRxMs = nowMs;
  memcpy(c.raw[sub - 1], d, 8);
  c.lastSubRxMs[sub - 1] = nowMs;
  c.frames[sub - 1]++;

  switch (sub) {
    case 1:
      c.balBits = d[0];
      c.status = d[1];
      for (int i = 0; i < TEMPS_PER_CMU; i++) c.tempRaw[i] = d[2 + i];
      break;
    case 2:
      storeCells(c, d, 0);
      if (c.cellCount == 0) c.cellCount = 4;
      break;
    case 3:
      if (storeCells(c, d, 4)) c.cellCount = 8;
      break;
    case 5:    // 0x6X5: byte 6-7 = CMU:ns egen mätning av modulspänningen i mV
      c.moduleMeasuredmV = be16(d + 6);
      break;
    default:   // 0x6X4: bara rådata (endast 0xFF hittills)
      break;
  }
  return true;
}

int16_t tempC(uint8_t raw, int16_t offset) {
  return raw == 0xFF ? TEMP_INVALID : (int16_t)(raw - offset);
}

bool cmuOnline(const CmuData& c, uint32_t timeoutMs, uint32_t nowMs) {
  return c.seen && (uint32_t)(nowMs - c.lastRxMs) < timeoutMs;
}

// Behåller förra min- eller maxplatsen om den cellen fortfarande är giltig och ligger inom
// STICKY_LOC_Q av det nya extremvärdet.
static void keepStickyLoc(const PackState& ps, const PackStats& prev, PackStats& o, bool isMin, uint32_t timeoutMs,
                          uint32_t nowMs) {
  const CellLoc pl = isMin ? prev.minLoc : prev.maxLoc;
  if (pl.module >= prev.moduleCount || pl.module >= o.moduleCount || prev.moduleIds[pl.module] != o.moduleIds[pl.module])
    return;
  const CmuData& pc = ps.cmu[o.moduleIds[pl.module]];
  if (!cmuOnline(pc, timeoutMs, nowMs) || pl.cell >= pc.cellCount || pc.cellmV[pl.cell] == 0) return;
  const CellLoc nl = isMin ? o.minLoc : o.maxLoc;
  const uint32_t pq = pc.cellQ[pl.cell], nq = ps.cmu[o.moduleIds[nl.module]].cellQ[nl.cell];
  if (isMin ? pq > nq + STICKY_LOC_Q : pq + STICKY_LOC_Q < nq) return;
  if (isMin) {
    o.minLoc = pl;
    o.minCellmV = pc.cellmV[pl.cell];
  } else {
    o.maxLoc = pl;
    o.maxCellmV = pc.cellmV[pl.cell];
  }
}

void computeStats(const PackState& ps, int16_t tempOffset, uint32_t cmuTimeoutMs, uint32_t nowMs, PackStats& o,
                  const PackStats* prev) {
  memset(&o, 0, sizeof(o));
  o.minCellmV = 0xFFFF;
  uint32_t minQ = 0xFFFFFFFFu, maxQ = 0;
  o.minTempC = TEMP_INVALID;
  o.maxTempC = TEMP_INVALID;
  bool anyTemp = false;

  for (uint8_t id = 1; id <= MAX_CMU_ID; id++) {
    const CmuData& c = ps.cmu[id];
    if (!c.seen) continue;
    const uint8_t m = o.moduleCount++;
    o.moduleIds[m] = id;
    if (!cmuOnline(c, cmuTimeoutMs, nowMs)) continue;
    o.online++;

    for (uint8_t k = 0; k < c.cellCount; k++) {
      const uint16_t v = c.cellmV[k];
      if (v == 0) continue;
      o.cellCount++;
      o.packmV += v;
      const uint32_t q = c.cellQ[k];
      if (q < minQ) { minQ = q; o.minCellmV = v; o.minLoc = {m, k}; }
      if (q > maxQ) { maxQ = q; o.maxCellmV = v; o.maxLoc = {m, k}; }
    }
    for (uint8_t t = 0; t < TEMPS_PER_CMU; t++) {
      const int16_t tc = tempC(c.tempRaw[t], tempOffset);
      if (tc == TEMP_INVALID) continue;
      if (!anyTemp || tc < o.minTempC) { o.minTempC = tc; o.minTempMod = m; }
      if (!anyTemp || tc > o.maxTempC) { o.maxTempC = tc; o.maxTempMod = m; }
      anyTemp = true;
    }
  }
  o.hasCells = o.cellCount > 0;
  if (!o.hasCells) o.minCellmV = 0;
  if (o.hasCells && prev && prev->hasCells) {
    keepStickyLoc(ps, *prev, o, true, cmuTimeoutMs, nowMs);
    keepStickyLoc(ps, *prev, o, false, cmuTimeoutMs, nowMs);
  }
  o.deltamV = o.hasCells ? (uint16_t)(o.maxCellmV - o.minCellmV) : 0;
}
