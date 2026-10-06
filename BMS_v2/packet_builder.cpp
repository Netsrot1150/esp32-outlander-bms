#include "packet_builder.h"
#include <string.h>

static int8_t toI8(int16_t t) {
  if (t == TEMP_INVALID) return BMS_TEMP_INVALID;
  if (t < -127) return -127;
  if (t > 127) return 127;
  return (int8_t)t;
}

static uint8_t locByte(CellLoc l) { return (uint8_t)((l.module << 4) | (l.cell & 0x0F)); }

void buildBmsPacket(BmsPacket& p, const PackState& ps, const PackStats& st, const BalanceDecision& d,
                    uint32_t alarmBits, uint32_t warnBits, int16_t tempOffset, uint32_t cmuTimeoutMs,
                    uint32_t nowMs, uint8_t seq, uint16_t socX10, int16_t currentdA, uint8_t socMode) {
  memset(&p, 0, sizeof(p));
  p.magic[0] = 'B';
  p.magic[1] = 'M';
  p.version = BMS_PACKET_VERSION;
  p.seq = seq;
  p.uptimeS = nowMs / 1000;
  p.packmV = st.packmV;
  p.minCellmV = st.minCellmV;
  p.maxCellmV = st.maxCellmV;
  p.minCellLoc = locByte(st.minLoc);
  p.maxCellLoc = locByte(st.maxLoc);
  p.minTempC = toI8(st.minTempC);
  p.maxTempC = toI8(st.maxTempC);
  p.balState = d.state;
  p.balTargetmV = d.targetmV;
  p.alarmBits = alarmBits;
  p.warnBits = warnBits;

  const uint8_t n = st.moduleCount < BMS_PACKET_MAX_CMU ? st.moduleCount : BMS_PACKET_MAX_CMU;
  p.cmuCount = n;
  for (uint8_t i = 0; i < n; i++) {
    const uint8_t id = st.moduleIds[i];
    const CmuData& c = ps.cmu[id];
    BmsCmuPacket& o = p.cmu[i];
    o.id = id;
    o.flags = (uint8_t)((cmuOnline(c, cmuTimeoutMs, nowMs) ? 0x01 : 0) | (c.cellCount == 8 ? 0x02 : 0));
    o.balBits = c.balBits;
    for (uint8_t k = 0; k < 8; k++) o.cellmV[k] = c.cellmV[k];
    for (uint8_t t = 0; t < 6; t++) o.tempC[t] = toI8(tempC(c.tempRaw[t], tempOffset));
  }
  p.socX10 = socX10;
  p.currentdA = currentdA;
  p.socMode = socMode;
}
