#include "test_framework.h"
#include "current.h"
#include <cmath>

static Settings cs() { Settings s; settingsDefaults(s); s.curEnabled = true; return s; }
static bool near(float a, float b, float tol = 0.05f) { return std::fabs(a - b) <= tol; }

TEST(strom_avstangd_ger_noll) {
  Settings s; settingsDefaults(s);
  const CurrentStatus c = currentFromMv(1000, s);
  CHECK(!c.enabled); CHECK(near(c.amps, 0)); CHECK(!c.saturated);
}

TEST(strom_nollpunkt_och_tecken) {
  Settings s = cs();   // 22,00 mV/A, nollpunkt 1650 mV
  CHECK(near(currentFromMv(1650, s).amps, 0));
  CHECK(near(currentFromMv(1650 - 220, s).amps, -10));   // sjunkande spänning = urladdning
  CHECK(near(currentFromMv(1650 + 110, s).amps, 5));
  s.curInvert = true;
  CHECK(near(currentFromMv(1650 - 220, s).amps, 10));
}

TEST(strom_faktor_och_kanslighet) {
  Settings s = cs(); s.curFactorX10 = 20; s.curSensX100 = 3300;
  CHECK(near(currentFromMv(1650 - 330, s).amps, -20));
}

TEST(strom_mattnad) {
  Settings s = cs();
  CurrentStatus c = currentFromMv(30, s);
  CHECK(c.saturated); CHECK(near(c.amps, -70));
  c = currentFromMv(150, s);
  CHECK(!c.saturated); CHECK(near(c.amps, -68.18f, 0.1f));
  c = currentFromMv(3200, s);
  CHECK(c.saturated); CHECK(near(c.amps, 70));
}
