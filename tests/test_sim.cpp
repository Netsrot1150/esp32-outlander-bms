#include "test_framework.h"
#include "sim.h"
#include "cmu.h"

struct SimCtx { PackState ps; uint32_t now; int frames; };

static void toPack(uint16_t id, const uint8_t d[8], void* ctx) {
  SimCtx* c = (SimCtx*)ctx;
  decodeFrame(c->ps, id, d, 8, c->now);
  c->frames++;
}

TEST(sim_ger_8_cmu_med_8_celler) {
  SimCtx c; packInit(c.ps); c.now = 1000; c.frames = 0;
  simReset();
  simStep(3901, 0, c.now, toPack, &c);
  CHECK_EQ(c.frames, 24);
  PackStats st; computeStats(c.ps, 40, 2000, c.now, st);
  CHECK_EQ(st.moduleCount, 8); CHECK_EQ(st.online, 8); CHECK_EQ(st.cellCount, 64);
  CHECK(st.minCellmV >= 3900 && st.maxCellmV <= 3960);
  CHECK(st.minTempC >= 20 && st.maxTempC <= 22);
  for (int id = 1; id <= 8; id++) CHECK_EQ(c.ps.cmu[id].balBits, 0);
}

TEST(sim_bleed_sanker_celler_over_malet) {
  SimCtx c; packInit(c.ps); c.frames = 0;
  simReset();
  c.now = 0;
  simStep(3940, 1, c.now, toPack, &c);
  CHECK_EQ(c.ps.cmu[3].balBits, 0x10);        // modul 3, cell 5 (3955 mV) ligger över målet
  for (uint32_t t = 1; t <= 200; t++) { c.now = t * 1000; simStep(3940, 1, c.now, toPack, &c); }
  CHECK_EQ(c.ps.cmu[3].balBits, 0);
  CHECK(c.ps.cmu[3].cellmV[4] < 3945);
}
