#include "settings_json.h"
#include <limits>
#include <stdio.h>
#include <string.h>

static const char PASS_MASK[] = "********";

static void ipStr(const uint8_t ip[4], char* out, size_t n) {
  snprintf(out, n, "%u.%u.%u.%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);
}

void settingsToJson(const Settings& s, JsonObject o, bool mask) {
  o["balMode"] = s.balMode;
  o["balStartmV"] = s.balStartmV;
  o["balStartDeltamV"] = s.balStartDeltamV;
  o["balStopDeltamV"] = s.balStopDeltamV;
  o["balMaxTempC"] = s.balMaxTempC;
  o["balTargetOffsetmV"] = s.balTargetOffsetmV;
  o["manualTargetmV"] = s.manualTargetmV;
  o["manualTimeoutMin"] = s.manualTimeoutMin;
  o["cellHighWarnmV"] = s.cellHighWarnmV;
  o["cellHighAlarmmV"] = s.cellHighAlarmmV;
  o["cellLowWarnmV"] = s.cellLowWarnmV;
  o["cellLowAlarmmV"] = s.cellLowAlarmmV;
  o["deltaWarnmV"] = s.deltaWarnmV;
  o["deltaAlarmmV"] = s.deltaAlarmmV;
  o["tempHighWarnC"] = s.tempHighWarnC;
  o["tempHighAlarmC"] = s.tempHighAlarmC;
  o["tempLowWarnC"] = s.tempLowWarnC;
  o["tempOffset"] = s.tempOffset;
  o["canPollMs"] = s.canPollMs;
  o["expectedCmus"] = s.expectedCmus;
  o["cmuTimeoutMs"] = s.cmuTimeoutMs;
  o["curEnabled"] = s.curEnabled;
  o["curInvert"] = s.curInvert;
  o["curSensX100"] = s.curSensX100;
  o["curZeromV"] = s.curZeromV;
  o["curFactorX10"] = s.curFactorX10;
  o["curWarnA"] = s.curWarnA;
  o["curAlarmA"] = s.curAlarmA;
  o["capacityAhX10"] = s.capacityAhX10;
  o["socEmptymV"] = s.socEmptymV;
  o["socFullmV"] = s.socFullmV;
  o["apSsid"] = s.apSsid;
  o["apPass"] = mask ? PASS_MASK : s.apPass;
  o["staSsid"] = s.staSsid;
  o["staPass"] = (mask && s.staPass[0]) ? PASS_MASK : s.staPass;
  o["staStatic"] = s.staStatic;
  char ip[16];
  ipStr(s.staIp, ip, sizeof(ip));
  o["staIp"] = ip;
  ipStr(s.staGw, ip, sizeof(ip));
  o["staGw"] = ip;
  ipStr(s.staMask, ip, sizeof(ip));
  o["staMask"] = ip;
  o["espnowEnabled"] = s.espnowEnabled;
}

template <typename T>
static bool readNum(JsonObjectConst o, const char* key, T& dst) {
  JsonVariantConst v = o[key];
  if (v.isNull()) return true;
  if (!v.is<long>()) return false;
  const long x = v.as<long>();
  if (x < (long)std::numeric_limits<T>::min() || x > (long)std::numeric_limits<T>::max()) return false;
  dst = (T)x;
  return true;
}

static bool readBool(JsonObjectConst o, const char* key, bool& dst) {
  JsonVariantConst v = o[key];
  if (v.isNull()) return true;
  if (!v.is<bool>()) return false;
  dst = v.as<bool>();
  return true;
}

static bool readStr(JsonObjectConst o, const char* key, char* dst, size_t n, bool isPassword) {
  JsonVariantConst v = o[key];
  if (v.isNull()) return true;
  if (!v.is<const char*>()) return false;
  const char* str = v.as<const char*>();
  if (isPassword && strcmp(str, PASS_MASK) == 0) return true;   // oförändrat
  const size_t len = strlen(str);
  if (len >= n) return false;
  memcpy(dst, str, len + 1);
  return true;
}

static bool readIp(JsonObjectConst o, const char* key, uint8_t dst[4]) {
  JsonVariantConst v = o[key];
  if (v.isNull()) return true;
  if (!v.is<const char*>()) return false;
  unsigned a, b, c, d;
  char extra;
  if (sscanf(v.as<const char*>(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &extra) != 4) return false;
  if (a > 255 || b > 255 || c > 255 || d > 255) return false;
  dst[0] = (uint8_t)a; dst[1] = (uint8_t)b; dst[2] = (uint8_t)c; dst[3] = (uint8_t)d;
  return true;
}

#define NUMF(f)      if (!readNum(o, #f, t.f)) return #f
#define BOOLF(f)     if (!readBool(o, #f, t.f)) return #f
#define STRF(f, pw)  if (!readStr(o, #f, t.f, sizeof(t.f), pw)) return #f
#define IPF(f)       if (!readIp(o, #f, t.f)) return #f

const char* settingsFromJson(Settings& s, JsonObjectConst o) {
  Settings t = s;
  NUMF(balMode); NUMF(balStartmV); NUMF(balStartDeltamV); NUMF(balStopDeltamV); NUMF(balMaxTempC);
  NUMF(balTargetOffsetmV); NUMF(manualTargetmV); NUMF(manualTimeoutMin);
  NUMF(cellHighWarnmV); NUMF(cellHighAlarmmV); NUMF(cellLowWarnmV); NUMF(cellLowAlarmmV);
  NUMF(deltaWarnmV); NUMF(deltaAlarmmV); NUMF(tempHighWarnC); NUMF(tempHighAlarmC); NUMF(tempLowWarnC);
  NUMF(tempOffset); NUMF(canPollMs); NUMF(expectedCmus); NUMF(cmuTimeoutMs);
  NUMF(curSensX100); NUMF(curZeromV); NUMF(curFactorX10); NUMF(curWarnA); NUMF(curAlarmA);
  NUMF(capacityAhX10); NUMF(socEmptymV); NUMF(socFullmV); BOOLF(curEnabled); BOOLF(curInvert);
  STRF(apSsid, false); STRF(apPass, true); STRF(staSsid, false); STRF(staPass, true);
  BOOLF(staStatic); IPF(staIp); IPF(staGw); IPF(staMask); BOOLF(espnowEnabled);
  const char* err = settingsValidate(t);
  if (err) return err;
  s = t;
  return nullptr;
}
