#include "test_framework.h"
#include "cmu.h"
#include "dump_frames.h"
#include <cstring>

TEST(cmu_avkodar_8_cells_cmu_ur_dump) {
  PackState ps; packInit(ps);
  feedDump(ps, 1000);
  const CmuData& c = ps.cmu[0x0B];
  CHECK(c.seen);
  CHECK_EQ(c.cellCount, 8);
  CHECK_EQ(c.cellmV[0], 3957); CHECK_EQ(c.cellmV[1], 3960); CHECK_EQ(c.cellmV[3], 3960);
  CHECK_EQ(c.cellmV[4], 3960); CHECK_EQ(c.cellmV[5], 3958); CHECK_EQ(c.cellmV[7], 3961);
  CHECK_EQ(c.balBits, 0);
  CHECK_EQ(c.status, 0x30);
  CHECK_EQ(c.tempRaw[0], 0x3C); CHECK_EQ(c.tempRaw[2], 0x3B); CHECK_EQ(c.tempRaw[5], 0xFF);
  CHECK_EQ(c.frames[3], 1);      // 0x6B4 räknas men tolkas inte
  CHECK_EQ(c.raw[3][0], 0xFF);
  CHECK_EQ(c.lastRxMs, 1000);
}

TEST(cmu_0x6X5_ger_cmuns_egen_modulspanning) {
  // Ur egen inspelning 2026-10-04 (CMU 2, 3 och 5).
  PackState ps; packInit(ps);
  const uint8_t m2[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x74};
  const uint8_t m3[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0C};
  CHECK(decodeFrame(ps, 0x625, m2, 8, 1000));
  CHECK(decodeFrame(ps, 0x635, m3, 8, 1000));
  CHECK_EQ(ps.cmu[2].moduleMeasuredmV, 30836);
  CHECK_EQ(ps.cmu[3].moduleMeasuredmV, 30732);
  CHECK_EQ(ps.cmu[2].frames[4], 1);
  CHECK_EQ(ps.cmu[2].raw[4][7], 0x74);
  CHECK(!decodeFrame(ps, 0x626, m2, 8, 1000));   // sub 6 finns inte
}

TEST(cmu_egen_inspelning_temperaturbytes) {
  PackState ps; packInit(ps);
  const uint8_t s1[8] = {0x00, 0x60, 0x52, 0x52, 0x51, 0x51, 0x51, 0xFF};
  CHECK(decodeFrame(ps, 0x621, s1, 8, 1000));
  CHECK_EQ(ps.cmu[2].status, 0x60);
  CHECK_EQ(tempC(ps.cmu[2].tempRaw[0], 52), 30);
  CHECK_EQ(tempC(ps.cmu[2].tempRaw[5], 52), TEMP_INVALID);
}

TEST(cmu_utan_0x6X3_har_4_celler) {
  PackState ps; packInit(ps);
  feedDump(ps, 1000);
  const CmuData& c = ps.cmu[1];
  CHECK_EQ(c.cellCount, 4);
  CHECK_EQ(c.cellmV[0], 3967); CHECK_EQ(c.cellmV[3], 3964);
  CHECK_EQ(c.cellmV[4], 0);
  CHECK_EQ(c.frames[2], 0);
}

TEST(cmu_temperatur_en_byte_per_givare) {
  CHECK_EQ(tempC(0x3B, 40), 19);
  CHECK_EQ(tempC(0x3C, 40), 20);
  CHECK_EQ(tempC(0x3B, 50), 9);
  CHECK_EQ(tempC(0xFF, 40), TEMP_INVALID);
  PackState ps; packInit(ps);
  CHECK_EQ(ps.cmu[3].tempRaw[0], 0xFF);   // ingen 0x6X1 ännu = ingen temperatur, inte -40 °C
}

TEST(cmu_ogiltig_cellspanning_ignoreras) {
  PackState ps; packInit(ps);
  feedDump(ps, 1000);
  const uint8_t bad[8] = {0xFF, 0xFF, 0x0F, 0x7B, 0x00, 0x10, 0x0F, 0x7C};   // 65535 och 16 mV är ogiltiga
  CHECK(decodeFrame(ps, 0x612, bad, 8, 1100));
  CHECK_EQ(ps.cmu[1].cellmV[0], 3967);   // föregående värde behålls
  CHECK_EQ(ps.cmu[1].cellmV[1], 3963);
  CHECK_EQ(ps.cmu[1].cellmV[2], 3964);
  CHECK_EQ(ps.cmu[1].invalidCells, 2);
  const uint8_t allBad[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  CHECK(decodeFrame(ps, 0x613, allBad, 8, 1200));
  CHECK_EQ(ps.cmu[1].cellCount, 4);      // en 0x6X3 utan giltiga celler gör inte modulen 8-cellig
}

TEST(cmu_ignorerar_andra_ramar) {
  PackState ps; packInit(ps);
  const uint8_t d[8] = {0};
  CHECK(!decodeFrame(ps, 0x3C3, d, 8, 0));
  CHECK(!decodeFrame(ps, 0x601, d, 8, 0));        // CMU-id 0
  CHECK(!decodeFrame(ps, 0x616, d, 8, 0));        // sub 6
  CHECK(!decodeFrame(ps, 0x612, d, 7, 0));        // fel längd
  CHECK(!decodeFrame(ps, 0x18FF0612, d, 8, 0));   // extended id
  CHECK(!ps.cmu[1].seen);
}

TEST(cmu_statistik_over_packet) {
  PackState ps; packInit(ps);
  feedDump(ps, 1000);
  PackStats st; computeStats(ps, 40, 2000, 1500, st);
  CHECK_EQ(st.moduleCount, 2);
  CHECK_EQ(st.moduleIds[0], 1); CHECK_EQ(st.moduleIds[1], 0x0B);
  CHECK_EQ(st.online, 2);
  CHECK_EQ(st.cellCount, 12);
  CHECK_EQ(st.packmV, 47532);
  CHECK_EQ(st.minCellmV, 3957); CHECK_EQ(st.minLoc.module, 1); CHECK_EQ(st.minLoc.cell, 0);
  CHECK_EQ(st.maxCellmV, 3967); CHECK_EQ(st.maxLoc.module, 0); CHECK_EQ(st.maxLoc.cell, 0);
  CHECK_EQ(st.deltamV, 10);
  CHECK_EQ(st.minTempC, 19); CHECK_EQ(st.minTempMod, 0);
  CHECK_EQ(st.maxTempC, 20); CHECK_EQ(st.maxTempMod, 1);
  CHECK(st.hasCells);
}

TEST(cmu_offline_raknas_inte_i_statistiken) {
  PackState ps; packInit(ps);
  feedDump(ps, 1000);
  PackStats st; computeStats(ps, 40, 2000, 2999, st);
  CHECK_EQ(st.online, 2);
  computeStats(ps, 40, 2000, 3001, st);
  CHECK_EQ(st.moduleCount, 2);
  CHECK_EQ(st.online, 0);
  CHECK_EQ(st.cellCount, 0);
  CHECK(!st.hasCells);
  CHECK_EQ(st.minCellmV, 0);
  CHECK_EQ(st.deltamV, 0);
  CHECK_EQ(st.minTempC, TEMP_INVALID);
}

TEST(cmu_online_klarar_millis_overslag) {
  CmuData c; std::memset(&c, 0, sizeof(c));
  c.seen = true; c.lastRxMs = 0xFFFFFF00u;
  CHECK(cmuOnline(c, 2000, 0x50));
  CHECK(!cmuOnline(c, 2000, 0x900));
}

static void cells612(PackState& ps, uint16_t v, uint32_t now) {
  const uint8_t d[8] = {(uint8_t)(v >> 8), (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)v,
                        (uint8_t)(v >> 8), (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)v};
  decodeFrame(ps, 0x612, d, 8, now);
}

TEST(cmu_cellspanning_jamnas_ut) {
  PackState ps; packInit(ps);
  cells612(ps, 4000, 0);
  CHECK_EQ(ps.cmu[1].cellmV[0], 4000);       // första värdet tas direkt
  cells612(ps, 4010, 40);
  CHECK_EQ(ps.cmu[1].cellmV[0], 4000);       // ett enstaka steg på 10 mV syns knappt
  CHECK_EQ(ps.cmu[1].cellRawmV[0], 4010);
  for (int i = 0; i < 99; i++) cells612(ps, 4010, 80 + 40 * i);
  CHECK_EQ(ps.cmu[1].cellmV[0], 4006);       // tidskonstant 100 ramar (4 s vid 25 Hz): 63 %
  for (int i = 0; i < 1000; i++) cells612(ps, 4010, 4000 + 40 * i);
  CHECK_EQ(ps.cmu[1].cellmV[0], 4010);
  for (int i = 0; i < 1000; i++) cells612(ps, i % 2 ? 4008 : 4009, 50000 + 40 * i);
  CHECK(ps.cmu[1].cellmV[0] == 4008 || ps.cmu[1].cellmV[0] == 4009);   // 1 mV-fladder blir medelvärdet
}

// Två moduler (CMU 1 och 2) med fyra celler var.
static void twoModules(PackState& ps, const uint16_t a[4], const uint16_t b[4]) {
  const uint16_t* v[2] = {a, b};
  for (int m = 0; m < 2; m++) {
    uint8_t d[8];
    for (int i = 0; i < 4; i++) { d[2 * i] = (uint8_t)(v[m][i] >> 8); d[2 * i + 1] = (uint8_t)v[m][i]; }
    decodeFrame(ps, 0x612 + 0x10 * m, d, 8, 1000);
  }
}

TEST(cmu_min_max_plats_byter_inte_vid_lika_varden) {
  PackState ps; packInit(ps);
  const uint16_t a[4] = {3950, 4000, 4010, 4005}, b[4] = {4001, 3950, 4010, 4002};
  twoModules(ps, a, b);
  PackStats first; computeStats(ps, 40, 2000, 1000, first);
  CHECK_EQ(first.minLoc.module, 0); CHECK_EQ(first.minLoc.cell, 0);
  CHECK_EQ(first.maxLoc.module, 0); CHECK_EQ(first.maxLoc.cell, 2);
  PackStats prev = first;
  prev.minLoc = {1, 1};
  prev.maxLoc = {1, 2};
  PackStats st; computeStats(ps, 40, 2000, 1000, st, &prev);
  CHECK_EQ(st.minLoc.module, 1); CHECK_EQ(st.minLoc.cell, 1);   // lika låg: förra platsen behålls
  CHECK_EQ(st.maxLoc.module, 1); CHECK_EQ(st.maxLoc.cell, 2);
  CHECK_EQ(st.minCellmV, 3950); CHECK_EQ(st.deltamV, 60);
}

TEST(cmu_min_plats_byter_nar_annan_cell_ar_tydligt_lagre) {
  PackState ps; packInit(ps);
  const uint16_t a[4] = {3949, 4000, 4010, 4005}, b[4] = {4001, 3950, 4010, 4002};
  twoModules(ps, a, b);
  PackStats prev; computeStats(ps, 40, 2000, 1000, prev);
  prev.minLoc = {1, 1};
  PackStats st; computeStats(ps, 40, 2000, 1000, st, &prev);
  CHECK_EQ(st.minLoc.module, 0); CHECK_EQ(st.minLoc.cell, 0);   // 1 mV lägre räcker (hysteres 0,5 mV)
  CHECK_EQ(st.minCellmV, 3949);
}
