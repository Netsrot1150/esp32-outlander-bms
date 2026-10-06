#include "test_framework.h"
#include "settings.h"
#include "config.h"
#include <cstddef>
#include <cstring>

TEST(settings_standardvarden_ar_giltiga) {
  Settings s; settingsDefaults(s);
  CHECK(settingsValidate(s) == nullptr);
  CHECK_EQ(s.balMode, BAL_AUTO);
  CHECK_EQ(s.balStartmV, 3900); CHECK_EQ(s.balStartDeltamV, 20); CHECK_EQ(s.balStopDeltamV, 8);
  CHECK_EQ(s.balMaxTempC, 45); CHECK_EQ(s.balTargetOffsetmV, 5);
  CHECK_EQ(s.manualTargetmV, 3950); CHECK_EQ(s.manualTimeoutMin, 60);
  CHECK_EQ(s.cellHighWarnmV, 4150); CHECK_EQ(s.cellHighAlarmmV, 4200);
  CHECK_EQ(s.cellLowWarnmV, 3300); CHECK_EQ(s.cellLowAlarmmV, 3000);
  CHECK_EQ(s.deltaWarnmV, 50); CHECK_EQ(s.deltaAlarmmV, 100);
  CHECK_EQ(s.tempHighWarnC, 45); CHECK_EQ(s.tempHighAlarmC, 55); CHECK_EQ(s.tempLowWarnC, 0);
  CHECK_EQ(s.tempOffset, 52); CHECK_EQ(s.canPollMs, 200); CHECK_EQ(s.expectedCmus, 8); CHECK_EQ(s.cmuTimeoutMs, 2000);
  CHECK(std::strcmp(s.apSsid, DEFAULT_AP_SSID) == 0);
  CHECK(std::strcmp(s.staSsid, DEFAULT_STA_SSID) == 0);
  CHECK(s.staStatic);
  CHECK_EQ(s.staIp[0], 192); CHECK_EQ(s.staIp[3], 89); CHECK_EQ(s.staGw[3], 99); CHECK_EQ(s.staMask[0], 255);
  CHECK(s.espnowEnabled);
}

TEST(settings_validering_hittar_fel) {
  struct Case { void (*mut)(Settings&); const char* field; };
  const Case cases[] = {
    {[](Settings& s) { s.balMode = 3; }, "balMode"},
    {[](Settings& s) { s.balStartmV = 2900; }, "balStartmV"},
    {[](Settings& s) { s.balStopDeltamV = 20; }, "balStopDeltamV"},
    {[](Settings& s) { s.balMaxTempC = 80; }, "balMaxTempC"},
    {[](Settings& s) { s.cellHighWarnmV = 4200; }, "cellHighWarnmV"},
    {[](Settings& s) { s.cellLowWarnmV = 3000; }, "cellLowAlarmmV"},
    {[](Settings& s) { s.deltaAlarmmV = 40; }, "deltaWarnmV"},
    {[](Settings& s) { s.tempHighAlarmC = 40; }, "tempHighWarnC"},
    {[](Settings& s) { s.tempOffset = 101; }, "tempOffset"},
    {[](Settings& s) { s.canPollMs = 20; }, "canPollMs"},
    {[](Settings& s) { s.cmuTimeoutMs = 300; }, "cmuTimeoutMs"},
    {[](Settings& s) { s.canPollMs = 1000; s.cmuTimeoutMs = 1500; }, "cmuTimeoutMs"},
    {[](Settings& s) { s.expectedCmus = 0; }, "expectedCmus"},
    {[](Settings& s) { s.apSsid[0] = 0; }, "apSsid"},
    {[](Settings& s) { std::strcpy(s.apPass, "kort"); }, "apPass"},
    {[](Settings& s) { std::strcpy(s.staPass, "12345"); }, "staPass"},
    {[](Settings& s) { s.curWarnA = 70; }, "curWarnA"},
    {[](Settings& s) { s.socEmptymV = 4100; }, "socEmptymV"},
  };
  for (const Case& c : cases) {
    Settings s; settingsDefaults(s);
    c.mut(s);
    const char* err = settingsValidate(s);
    const bool ok = err != nullptr && std::strcmp(err, c.field) == 0;
    CHECK(ok);
    if (!ok) std::printf("    vantade %s, fick %s\n", c.field, err ? err : "OK");
  }
}

TEST(settings_tomt_hemnat_ar_tillatet) {
  Settings s; settingsDefaults(s);
  s.staSsid[0] = 0;
  s.staPass[0] = 0;
  CHECK(settingsValidate(s) == nullptr);
}

TEST(settings_aterstall_bara_natverk) {
  Settings s; settingsDefaults(s);
  std::strcpy(s.apPass, "annat-losen");
  s.balStartmV = 3950;
  settingsDefaultsNetwork(s);
  CHECK(std::strcmp(s.apPass, DEFAULT_AP_PASS) == 0);
  CHECK_EQ(s.balStartmV, 3950);
}

TEST(settings_v2_falt_och_layout) {
  CHECK_EQ(offsetof(Settings, curSensX100), SETTINGS_V1_SIZE);
  Settings s; settingsDefaults(s);
  CHECK_EQ(s.curSensX100, 2200); CHECK_EQ(s.curZeromV, 1650); CHECK_EQ(s.curFactorX10, 10);
  CHECK_EQ(s.curWarnA, 60); CHECK_EQ(s.curAlarmA, 70); CHECK_EQ(s.capacityAhX10, 460);
  CHECK_EQ(s.socEmptymV, 3300); CHECK_EQ(s.socFullmV, 4100);
  CHECK(!s.curEnabled); CHECK(!s.curInvert);
  CHECK(settingsValidate(s) == nullptr);
}

TEST(settings_migrering_fran_v1) {
  Settings old; settingsDefaults(old);
  old.tempOffset = 65; old.balStartmV = 3920;
  unsigned char blob[SETTINGS_V1_SIZE];
  std::memcpy(blob, &old, SETTINGS_V1_SIZE);
  Settings out; std::memset(&out, 0, sizeof(out));
  CHECK(settingsFromBlob(out, blob, SETTINGS_V1_SIZE, 1));
  CHECK_EQ(out.tempOffset, 65); CHECK_EQ(out.balStartmV, 3920);
  CHECK_EQ(out.curSensX100, 2200); CHECK_EQ(out.socFullmV, 4100);
  CHECK(!settingsFromBlob(out, blob, SETTINGS_V1_SIZE, 2));
  CHECK(!settingsFromBlob(out, blob, 100, 1));
  Settings cur; settingsDefaults(cur); cur.capacityAhX10 = 380;
  CHECK(settingsFromBlob(out, &cur, sizeof(cur), SETTINGS_VERSION));
  CHECK_EQ(out.capacityAhX10, 380);
}
