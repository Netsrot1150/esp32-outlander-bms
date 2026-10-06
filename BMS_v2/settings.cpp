#include "settings.h"
#include "config.h"
#include <string.h>

static void copyStr(char* dst, size_t n, const char* src) {
  strncpy(dst, src, n - 1);
  dst[n - 1] = 0;
}

void settingsDefaultsNetwork(Settings& s) {
  copyStr(s.apSsid, sizeof(s.apSsid), DEFAULT_AP_SSID);
  copyStr(s.apPass, sizeof(s.apPass), DEFAULT_AP_PASS);
  copyStr(s.staSsid, sizeof(s.staSsid), DEFAULT_STA_SSID);
  copyStr(s.staPass, sizeof(s.staPass), DEFAULT_STA_PASS);
  s.staStatic = true;
  const uint8_t ip[4] = {192, 168, 1, 89}, gw[4] = {192, 168, 1, 99}, mask[4] = {255, 255, 255, 0};
  memcpy(s.staIp, ip, 4);
  memcpy(s.staGw, gw, 4);
  memcpy(s.staMask, mask, 4);
  s.espnowEnabled = true;
}

void settingsDefaults(Settings& s) {
  memset(&s, 0, sizeof(s));
  s.balMode = BAL_AUTO;
  s.balStartmV = 3900;
  s.balStartDeltamV = 20;
  s.balStopDeltamV = 8;
  s.balMaxTempC = 45;
  s.balTargetOffsetmV = 5;
  s.manualTargetmV = 3950;
  s.manualTimeoutMin = 60;
  s.cellHighWarnmV = 4150;
  s.cellHighAlarmmV = 4200;
  s.cellLowWarnmV = 3300;
  s.cellLowAlarmmV = 3000;
  s.deltaWarnmV = 50;
  s.deltaAlarmmV = 100;
  s.tempHighWarnC = 45;
  s.tempHighAlarmC = 55;
  s.tempLowWarnC = 0;
  s.tempOffset = 52;   // T ≈ rå − 52 (Tom de Brees kalibrering), verifieras med IR-mätning
  s.canPollMs = 200;
  s.expectedCmus = 8;
  s.cmuTimeoutMs = 2000;
  s.curSensX100 = 2200;     // WCS1700 vid 3,3 V matning
  s.curZeromV = 1650;
  s.curFactorX10 = 10;
  s.curWarnA = 60;
  s.curAlarmA = 70;
  s.capacityAhX10 = 460;    // LEV46
  s.socEmptymV = 3300;
  s.socFullmV = 4100;
  s.curEnabled = false;
  s.curInvert = false;
#ifdef SIMULATE_CMUS
  s.curEnabled = true;      // simulerad ström i testbygget
#endif
  settingsDefaultsNetwork(s);
}

#define REQ(cond, name) do { if (!(cond)) return name; } while (0)

const char* settingsValidate(const Settings& s) {
  REQ(s.balMode <= BAL_MANUAL, "balMode");
  REQ(s.balStartmV >= 3000 && s.balStartmV <= 4200, "balStartmV");
  REQ(s.balStartDeltamV >= 5 && s.balStartDeltamV <= 200, "balStartDeltamV");
  REQ(s.balStopDeltamV >= 1 && s.balStopDeltamV < s.balStartDeltamV, "balStopDeltamV");
  REQ(s.balMaxTempC >= 20 && s.balMaxTempC <= 70, "balMaxTempC");
  REQ(s.balTargetOffsetmV <= 50, "balTargetOffsetmV");
  REQ(s.manualTargetmV >= 3000 && s.manualTargetmV <= 4200, "manualTargetmV");
  REQ(s.manualTimeoutMin >= 1 && s.manualTimeoutMin <= 600, "manualTimeoutMin");
  REQ(s.cellHighWarnmV >= 3500 && s.cellHighWarnmV < s.cellHighAlarmmV, "cellHighWarnmV");
  REQ(s.cellHighAlarmmV <= 4300, "cellHighAlarmmV");
  REQ(s.cellLowAlarmmV >= 2500 && s.cellLowAlarmmV < s.cellLowWarnmV, "cellLowAlarmmV");
  REQ(s.cellLowWarnmV <= 3900, "cellLowWarnmV");
  REQ(s.deltaWarnmV >= 5 && s.deltaWarnmV < s.deltaAlarmmV, "deltaWarnmV");
  REQ(s.deltaAlarmmV <= 2000, "deltaAlarmmV");
  REQ(s.tempHighWarnC >= 20 && s.tempHighWarnC < s.tempHighAlarmC, "tempHighWarnC");
  REQ(s.tempHighAlarmC <= 90, "tempHighAlarmC");
  REQ(s.tempLowWarnC >= -30 && s.tempLowWarnC <= 20, "tempLowWarnC");
  REQ(s.tempOffset >= 0 && s.tempOffset <= 100, "tempOffset");
  REQ(s.canPollMs >= 40 && s.canPollMs <= 1000, "canPollMs");
  REQ(s.expectedCmus >= 1 && s.expectedCmus <= MAX_CMU_ID, "expectedCmus");
  REQ(s.cmuTimeoutMs >= 500 && s.cmuTimeoutMs <= 10000 && s.cmuTimeoutMs >= 2u * s.canPollMs, "cmuTimeoutMs");
  REQ(s.curSensX100 >= 500 && s.curSensX100 <= 10000, "curSensX100");
  REQ(s.curZeromV >= 500 && s.curZeromV <= 2800, "curZeromV");
  REQ(s.curFactorX10 >= 1 && s.curFactorX10 <= 100, "curFactorX10");
  REQ(s.curWarnA >= 1 && s.curWarnA < s.curAlarmA, "curWarnA");
  REQ(s.curAlarmA <= 500, "curAlarmA");
  REQ(s.capacityAhX10 >= 10 && s.capacityAhX10 <= 2000, "capacityAhX10");
  REQ(s.socEmptymV >= 2800 && s.socEmptymV < s.socFullmV, "socEmptymV");
  REQ(s.socFullmV >= 3800 && s.socFullmV <= 4250, "socFullmV");
  size_t n = strnlen(s.apSsid, sizeof(s.apSsid));
  REQ(n >= 1 && n <= 32, "apSsid");
  n = strnlen(s.apPass, sizeof(s.apPass));
  REQ(n >= 8 && n <= 63, "apPass");
  n = strnlen(s.staSsid, sizeof(s.staSsid));
  REQ(n <= 32, "staSsid");
  n = strnlen(s.staPass, sizeof(s.staPass));
  REQ(n == 0 || (n >= 8 && n <= 63), "staPass");
  return nullptr;
}

bool settingsFromBlob(Settings& out, const void* blob, size_t n, uint16_t ver) {
  Settings t;
  settingsDefaults(t);
  if (ver == SETTINGS_VERSION && n == sizeof(Settings)) memcpy(&t, blob, n);
  else if (ver == 1 && n == SETTINGS_V1_SIZE) memcpy(&t, blob, n);
  else return false;
  if (settingsValidate(t) != nullptr) return false;
  out = t;
  return true;
}
