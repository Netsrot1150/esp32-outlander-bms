#include "test_framework.h"
#include "soc.h"
#include <cmath>
#include <cstring>

static bool near(float a, float b, float tol = 0.2f) { return std::fabs(a - b) <= tol; }
static PackStats ps(uint16_t minmV, uint16_t maxmV) {
  PackStats p; std::memset(&p, 0, sizeof(p));
  p.hasCells = true; p.cellCount = 64; p.minCellmV = minmV; p.maxCellmV = maxmV;
  p.packmV = 64u * (minmV + maxmV) / 2;
  return p;
}
static Settings def() { Settings s; settingsDefaults(s); return s; }

TEST(soc_ocv_tabell_lev46) {
  CHECK(near(ocvPct(2750), 0)); CHECK(near(ocvPct(2600), 0));
  CHECK(near(ocvPct(4100), 100)); CHECK(near(ocvPct(4200), 100));
  CHECK(near(ocvPct(3300), 3)); CHECK(near(ocvPct(3800), 30)); CHECK(near(ocvPct(3825), 36));
  CHECK(near(ocvPct(4050), 88));
}

TEST(soc_ocv_omskalning) {
  Settings s = def();   // 3300 mV = 0 %, 4100 mV = 100 %
  CHECK(near(socFromOcv(3300, s), 0)); CHECK(near(socFromOcv(4100, s), 100));
  CHECK(near(socFromOcv(3800, s), (30.0f - 3) / 97 * 100));
  s.socFullmV = 3900;   // 53 % i tabellen blir 100 %
  CHECK(near(socFromOcv(3900, s), 100));
  CHECK(near(socFromOcv(3800, s), (30.0f - 3) / 50 * 100));
}

TEST(soc_energiprocent) {
  const Settings s = def();
  CHECK(near(socEnergyPct(0, s), 0, 0.01f)); CHECK(near(socEnergyPct(100, s), 100, 0.01f));
  // Den nedre halvan har lägre spänning, så halva laddningen är mindre än halva energin.
  CHECK(near(socEnergyPct(50, s), 48.3f, 0.3f));
  CHECK(near(socEnergyPct(socFromOcv(4008, s), s), 77.0f, 0.3f));
  CHECK(socEnergyPct(10, s) < 10.0f);
}

TEST(soc_utan_strom_foljer_ocv_med_utjamning) {
  SocState st; socInit(st, -1); const Settings s = def();
  const float a = socFromOcv(3780, s), b = socFromOcv(3850, s);
  socUpdate(st, ps(3780, 3800), s, false, 0, 100, 1000);
  CHECK(st.valid); CHECK_EQ(st.mode, SOC_OCV); CHECK(near(st.pct, a));
  socUpdate(st, ps(3850, 3860), s, false, 0, 60000, 61000);   // en tidskonstant: halvvägs
  CHECK(near(st.pct, (a + b) / 2, 0.3f));
}

TEST(soc_ah_rakning) {
  SocState st; socInit(st, 50); Settings s = def(); s.curEnabled = true;   // 46 Ah
  for (uint32_t i = 1; i <= 360; i++) socUpdate(st, ps(3780, 3800), s, true, -4.6f, 10000, i * 10000);   // 1 h, -4,6 A
  CHECK_EQ(st.mode, SOC_AH);
  CHECK(near(st.pct, 40, 0.05f));
  CHECK(near((float)st.ahOut, 4.6f, 0.01f));
  CHECK(near((float)st.ahIn, 0, 0.001f));
  CHECK(near((float)st.kWhOut, 4.6f * 64 * 3.79f / 1000, 0.01f));
}

TEST(soc_full_efter_laddning) {
  SocState st; socInit(st, 90); Settings s = def(); s.curEnabled = true;
  uint32_t t = 1000;
  socUpdate(st, ps(4050, 4085), s, true, 10.0f, 1000, t);
  CHECK(st.chargedSinceFull);
  for (int i = 0; i < 59; i++) { t += 1000; socUpdate(st, ps(4060, 4090), s, true, 0.2f, 1000, t); }
  CHECK(st.pct < 99.0f);
  for (int i = 0; i < 3; i++) { t += 1000; socUpdate(st, ps(4060, 4090), s, true, 0.2f, 1000, t); }
  CHECK(near(st.pct, 100, 0.01f));
  CHECK(!st.chargedSinceFull);
}

TEST(soc_vila_korrigerar_mot_ocv) {
  SocState st; socInit(st, 80); Settings s = def(); s.curEnabled = true;
  uint32_t t = 0;
  for (int i = 0; i < 30 * 60 + 2; i++) { t += 1000; socUpdate(st, ps(3780, 3800), s, true, 0.1f, 1000, t); }
  CHECK(near(st.pct, socFromOcv(3780, s), 0.1f));
}

TEST(soc_energi_och_tid) {
  SocState st; socInit(st, 100); const Settings s = def();
  CHECK(near(socKwhLeft(st, s, 64), 11.02f, 0.03f));   // 46 Ah, 64 celler, 3,30–4,10 V enligt kurvan
  socInit(st, 0);
  CHECK(near(socKwhLeft(st, s, 64), 0, 0.001f));
  socInit(st, 50);
  CHECK(near(socKwhLeft(st, s, 64), 11.02f * 0.483f, 0.05f));
  CHECK_EQ(socMinutesLeft(st, s, true, -23.0f), 60);
  CHECK_EQ(socMinutesLeft(st, s, true, 11.5f), 120);
  CHECK_EQ(socMinutesLeft(st, s, true, 0.5f), -1);
  CHECK_EQ(socMinutesLeft(st, s, false, -20.0f), -1);
}

TEST(soc_sparat_varde) {
  SocState st; socInit(st, 72.5f);
  CHECK(st.valid); CHECK(near(st.pct, 72.5f, 0.01f));
  socInit(st, -1);
  CHECK(!st.valid);
}
