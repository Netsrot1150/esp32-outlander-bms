#include "test_framework.h"
#include "celllog.h"
#include <cstring>

static void setCells(PackState& ps, uint8_t id, uint16_t base, uint32_t now) {
  for (uint8_t sub = 2; sub <= 3; sub++) {
    uint8_t d[8];
    for (int i = 0; i < 4; i++) {
      const uint16_t v = (uint16_t)(base + (sub - 2) * 4 + i);
      d[2 * i] = (uint8_t)(v >> 8); d[2 * i + 1] = (uint8_t)v;
    }
    decodeFrame(ps, 0x600 | (id << 4) | sub, d, 8, now);
  }
}

TEST(celllog_post_ar_72_byte) {
  CHECK_EQ(sizeof(CellLogRec), 72);
  CHECK_EQ(CELLLOG_N, 336);
}

TEST(celllog_timmedel_per_cell) {
  PackState ps; packInit(ps);
  setCells(ps, 2, 4000, 1000);    // modul 0: 4000..4007
  setCells(ps, 3, 3950, 1000);    // modul 1: 3950..3957
  PackStats st; computeStats(ps, 40, 2000, 1000, st);
  CellAccum acc; cellAccumReset(acc);
  CellLogRec out;
  CHECK(!cellAccumMake(acc, 0, out));
  cellAccumAdd(acc, ps, st, 2000, 1000);
  for (uint32_t i = 0; i < 500; i++) setCells(ps, 2, 4010, 1000);   // utjämnat ≈ 4010
  cellAccumAdd(acc, ps, st, 2000, 1000);
  CHECK(cellAccumMake(acc, 1700000000, out));
  CHECK_EQ(out.t, 1700000000u);
  CHECK_EQ(out.n, 16);
  CHECK_EQ(out.basemV, 3950);                // lägsta timmedel
  CHECK_EQ(out.off[0], 55);                  // (4000 + 4010) / 2 - 3950
  CHECK_EQ(out.off[7], 62);
  CHECK_EQ(out.off[8], 0);
  CHECK_EQ(out.off[15], 7);
  CHECK_EQ(out.off[16], CELLLOG_MISSING);    // ingen tredje modul
  CHECK_EQ(out.off[63], CELLLOG_MISSING);
}

TEST(celllog_stora_avvikelser_och_offline) {
  PackState ps; packInit(ps);
  setCells(ps, 2, 4100, 1000);
  setCells(ps, 3, 3700, 1000);
  setCells(ps, 4, 3900, 0);       // tyst sedan 0 ms: offline vid 5000 ms
  PackStats st; computeStats(ps, 40, 2000, 5000, st);
  CellAccum acc; cellAccumReset(acc);
  cellAccumAdd(acc, ps, st, 2000, 5000);
  CellLogRec out;
  CHECK(!cellAccumMake(acc, 1, out));        // alla CMU:er offline vid 5000 ms
  computeStats(ps, 40, 2000, 1500, st);
  cellAccumReset(acc);
  cellAccumAdd(acc, ps, st, 2000, 1500);
  CHECK(cellAccumMake(acc, 1, out));
  CHECK_EQ(out.basemV, 3700);
  CHECK_EQ(out.off[0], CELLLOG_MAX_OFF);     // 400 mV begränsas till 254
  CHECK_EQ(out.off[16], 200);
}

TEST(celllog_ring_och_relativ_tid) {
  static CellLogRec store[3];
  CellLogRing r; clInit(r, store, 3);
  CellLogRec rec; std::memset(&rec, 0, sizeof(rec));
  for (uint32_t i = 1; i <= 4; i++) { rec.t = histStamp(false, 0, i * 100); clPush(r, rec); }
  CHECK_EQ(r.count, 3);
  CHECK_EQ(clAt(r, 0).t, 200u | HIST_REL_FLAG);
  clFixRelative(r, 1700001000, 1000);
  CHECK_EQ(clAt(r, 0).t, 1700000200u);
  CHECK_EQ(clAt(r, 2).t, 1700000400u);
  CellLogRing r2; static CellLogRec store2[3]; clInit(r2, store2, 3);
  const CellLogRec in[2] = {clAt(r, 0), rec};   // rec har fortfarande relativ tid
  clLoad(r2, in, 2);
  CHECK_EQ(r2.count, 1);
}
