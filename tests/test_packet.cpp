#include "test_framework.h"
#include "packet_builder.h"
#include "dump_frames.h"

TEST(paket_storlek) {
  CHECK_EQ(sizeof(BmsCmuPacket), 25);
  CHECK_EQ(sizeof(BmsPacket), 237);
}

TEST(paket_byggs_fran_dump) {
  PackState ps; packInit(ps);
  feedDump(ps, 1000);
  PackStats st; computeStats(ps, 40, 2000, 1500, st);
  Settings s; settingsDefaults(s);
  BalancerMemory m; balancerInit(m);
  const BalanceDecision d = balancerDecide(st, s, m, 1500);
  BmsPacket p;
  buildBmsPacket(p, ps, st, d, 0x01, 0x20, 40, 2000, 1500, 7, 615, -123, 1);
  CHECK_EQ(p.magic[0], 'B'); CHECK_EQ(p.magic[1], 'M');
  CHECK_EQ(p.version, 2);
  CHECK_EQ(p.socX10, 615); CHECK_EQ(p.currentdA, -123); CHECK_EQ(p.socMode, 1);
  CHECK_EQ(p.version, BMS_PACKET_VERSION); CHECK_EQ(p.seq, 7); CHECK_EQ(p.uptimeS, 1);
  CHECK_EQ(p.packmV, 47532);
  CHECK_EQ(p.minCellmV, 3957); CHECK_EQ(p.maxCellmV, 3967);
  CHECK_EQ(p.minCellLoc, 0x10); CHECK_EQ(p.maxCellLoc, 0x00);
  CHECK_EQ(p.minTempC, 19); CHECK_EQ(p.maxTempC, 20);
  CHECK_EQ(p.balState, d.state); CHECK_EQ(p.balTargetmV, d.targetmV);
  CHECK_EQ(p.alarmBits, 0x01); CHECK_EQ(p.warnBits, 0x20);
  CHECK_EQ(p.cmuCount, 2);
  CHECK_EQ(p.cmu[0].id, 1); CHECK_EQ(p.cmu[0].flags, 0x01);
  CHECK_EQ(p.cmu[1].id, 11); CHECK_EQ(p.cmu[1].flags, 0x03);
  CHECK_EQ(p.cmu[1].cellmV[7], 3961);
  CHECK_EQ(p.cmu[1].tempC[0], 20);
  CHECK_EQ(p.cmu[1].tempC[5], BMS_TEMP_INVALID);
  CHECK_EQ(p.cmu[2].id, 0);
}
