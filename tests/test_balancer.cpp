#include "test_framework.h"
#include "balancer.h"
#include <cstring>

static PackStats stats(uint16_t minmV, uint16_t maxmV, uint8_t online = 8, int16_t maxT = 25) {
  PackStats st; std::memset(&st, 0, sizeof(st));
  st.moduleCount = 8; st.online = online; st.cellCount = 64; st.hasCells = true;
  st.minCellmV = minmV; st.maxCellmV = maxmV; st.deltamV = (uint16_t)(maxmV - minmV);
  st.minTempC = 20; st.maxTempC = maxT; st.packmV = 64u * minmV;
  return st;
}

static Settings mode(uint8_t m) { Settings s; settingsDefaults(s); s.balMode = m; return s; }

TEST(bal_av_skickar_lage_0) {
  BalancerMemory m; balancerInit(m);
  BalanceDecision d = balancerDecide(stats(3900, 3990), mode(BAL_OFF), m, 1000);
  CHECK_EQ(d.state, BS_OFF); CHECK_EQ(d.mode, 0); CHECK_EQ(d.targetmV, 3901);
  CHECK(std::strcmp(balStateName(d.state), "OFF") == 0);
}

TEST(bal_auto_vantar_under_startspanning) {
  BalancerMemory m; balancerInit(m);
  BalanceDecision d = balancerDecide(stats(3800, 3850), mode(BAL_AUTO), m, 1000);
  CHECK_EQ(d.state, BS_WAITING); CHECK_EQ(d.mode, 0);
  CHECK_STR_CONTAINS(d.reason, "3850");
}

TEST(bal_auto_vantar_pa_tillrackligt_delta) {
  BalancerMemory m; balancerInit(m);
  BalanceDecision d = balancerDecide(stats(3940, 3950), mode(BAL_AUTO), m, 1000);
  CHECK_EQ(d.state, BS_WAITING); CHECK_EQ(d.mode, 0);
  CHECK_STR_CONTAINS(d.reason, "delta 10");
}

TEST(bal_auto_startar_och_satter_mal) {
  BalancerMemory m; balancerInit(m);
  BalanceDecision d = balancerDecide(stats(3920, 3950), mode(BAL_AUTO), m, 1000);
  CHECK_EQ(d.state, BS_ACTIVE); CHECK_EQ(d.mode, 1); CHECK_EQ(d.targetmV, 3925);
}

TEST(bal_auto_mal_aldrig_under_startspanning) {
  BalancerMemory m; balancerInit(m);
  BalanceDecision d = balancerDecide(stats(3880, 3920), mode(BAL_AUTO), m, 1000);
  CHECK_EQ(d.state, BS_ACTIVE); CHECK_EQ(d.targetmV, 3900);
}

TEST(bal_auto_hysteres) {
  BalancerMemory m; balancerInit(m);
  const Settings s = mode(BAL_AUTO);
  CHECK_EQ(balancerDecide(stats(3920, 3950), s, m, 1000).state, BS_ACTIVE);    // delta 30 startar
  CHECK_EQ(balancerDecide(stats(3935, 3950), s, m, 1100).state, BS_ACTIVE);    // delta 15 fortsätter
  BalanceDecision d = balancerDecide(stats(3942, 3950), s, m, 1200);           // delta 8 stoppar
  CHECK_EQ(d.state, BS_WAITING);
  CHECK_STR_CONTAINS(d.reason, "Klar");
  CHECK_EQ(balancerDecide(stats(3935, 3950), s, m, 1300).state, BS_WAITING);   // delta 15 räcker inte för omstart
  CHECK_EQ(balancerDecide(stats(3930, 3950), s, m, 1400).state, BS_ACTIVE);    // delta 20 startar igen
}

TEST(bal_auto_hysteres_pa_startspanningen) {
  // Under laddning brusar högsta cellen runt 3,90 V. Aktiv balansering ska inte släppa för 3,898 V.
  BalancerMemory m; balancerInit(m);
  const Settings s = mode(BAL_AUTO);
  CHECK_EQ(balancerDecide(stats(3856, 3900), s, m, 1000).state, BS_ACTIVE);
  CHECK_EQ(balancerDecide(stats(3847, 3898), s, m, 1100).state, BS_ACTIVE);    // inom 10 mV
  CHECK_EQ(balancerDecide(stats(3847, 3891), s, m, 1200).state, BS_ACTIVE);
  CHECK_EQ(balancerDecide(stats(3847, 3889), s, m, 1300).state, BS_WAITING);   // under 3890 släpper
  CHECK_EQ(balancerDecide(stats(3847, 3898), s, m, 1400).state, BS_WAITING);   // start kräver åter 3900
}

TEST(bal_blockeras_av_offline_cmu) {
  BalancerMemory m; balancerInit(m);
  BalanceDecision d = balancerDecide(stats(3920, 3950, 7), mode(BAL_AUTO), m, 1000);
  CHECK_EQ(d.state, BS_BLOCKED); CHECK_EQ(d.mode, 0); CHECK_EQ(d.targetmV, 3901);
  CHECK_STR_CONTAINS(d.reason, "7 av 8");
  d = balancerDecide(stats(3800, 3850, 7), mode(BAL_MANUAL), m, 1000);
  CHECK_EQ(d.state, BS_BLOCKED);
}

TEST(bal_blockeras_av_hog_temperatur) {
  BalancerMemory m; balancerInit(m);
  const Settings s = mode(BAL_AUTO);
  BalanceDecision d = balancerDecide(stats(3920, 3950, 8, 46), s, m, 1000);
  CHECK_EQ(d.state, BS_BLOCKED);
  CHECK_STR_CONTAINS(d.reason, "46");
  CHECK_EQ(balancerDecide(stats(3920, 3950, 8, 45), s, m, 1100).state, BS_ACTIVE);
  CHECK_EQ(balancerDecide(stats(3920, 3950, 8, TEMP_INVALID), s, m, 1200).state, BS_ACTIVE);
}

TEST(bal_manuell_med_timeout) {
  BalancerMemory m; balancerInit(m);
  const Settings s = mode(BAL_MANUAL);   // mål 3950 mV, 60 min
  BalanceDecision d = balancerDecide(stats(3800, 3850), s, m, 1000);
  CHECK_EQ(d.state, BS_MANUAL); CHECK_EQ(d.mode, 1); CHECK_EQ(d.targetmV, 3950);
  CHECK(!d.manualExpired);
  CHECK_STR_CONTAINS(d.reason, "60 min");
  d = balancerDecide(stats(3800, 3850), s, m, 1000 + 3600000 - 1);
  CHECK_EQ(d.state, BS_MANUAL);
  d = balancerDecide(stats(3800, 3850), s, m, 1000 + 3600000);
  CHECK_EQ(d.state, BS_OFF); CHECK(d.manualExpired); CHECK_EQ(d.mode, 0);
}

TEST(bal_lagesbyte_nollstaller_hysteres) {
  BalancerMemory m; balancerInit(m);
  CHECK_EQ(balancerDecide(stats(3920, 3950), mode(BAL_AUTO), m, 1000).state, BS_ACTIVE);
  balancerDecide(stats(3935, 3950), mode(BAL_OFF), m, 1100);
  CHECK_EQ(balancerDecide(stats(3935, 3950), mode(BAL_AUTO), m, 1200).state, BS_WAITING);
}
