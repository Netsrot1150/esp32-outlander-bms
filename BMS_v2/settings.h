#pragma once
// Inställningar som sparas i NVS. Ren kod (ingen Arduino).
#include <stdint.h>
#include <stddef.h>

enum BalanceMode : uint8_t { BAL_OFF = 0, BAL_AUTO = 1, BAL_MANUAL = 2 };

static const uint16_t SETTINGS_VERSION = 2;   // öka när structen ändras
static const size_t   SETTINGS_V1_SIZE = 252; // storleken på v1 – nya fält läggs alltid efter detta

struct Settings {
  // Balansering
  uint8_t  balMode;             // BalanceMode
  uint16_t balStartmV;
  uint16_t balStartDeltamV;
  uint16_t balStopDeltamV;
  int16_t  balMaxTempC;
  uint16_t balTargetOffsetmV;
  uint16_t manualTargetmV;
  uint16_t manualTimeoutMin;
  // Larmgränser
  uint16_t cellHighWarnmV;
  uint16_t cellHighAlarmmV;
  uint16_t cellLowWarnmV;
  uint16_t cellLowAlarmmV;
  uint16_t deltaWarnmV;
  uint16_t deltaAlarmmV;
  int16_t  tempHighWarnC;
  int16_t  tempHighAlarmC;
  int16_t  tempLowWarnC;
  // Temperatur: T = rå - tempOffset
  int16_t  tempOffset;
  // CAN
  uint16_t canPollMs;
  uint8_t  expectedCmus;
  uint16_t cmuTimeoutMs;
  // Nätverk
  char     apSsid[33];
  char     apPass[65];
  char     staSsid[33];
  char     staPass[65];
  bool     staStatic;
  uint8_t  staIp[4];
  uint8_t  staGw[4];
  uint8_t  staMask[4];
  bool     espnowEnabled;
  // ---- v2: ström och laddnivå (måste ligga efter v1-fälten, se SETTINGS_V1_SIZE) ----
  uint16_t curSensX100;     // sensorns känslighet, mV/A × 100
  uint16_t curZeromV;       // utsignal vid 0 A
  uint16_t curFactorX10;    // antal ledare genom sensorn × 10
  uint16_t curWarnA;        // urladdningsström, varning
  uint16_t curAlarmA;       // urladdningsström, larm
  uint16_t capacityAhX10;   // kapacitet i Ah × 10
  uint16_t socEmptymV;      // 0 % vid lägsta cell
  uint16_t socFullmV;       // 100 % vid lägsta cell
  bool     curEnabled;
  bool     curInvert;
};

void settingsDefaults(Settings& s);
void settingsDefaultsNetwork(Settings& s);       // återställer bara nätverksfälten
const char* settingsValidate(const Settings& s); // nullptr = OK, annars namnet på första ogiltiga fältet
// Läser en sparad blob (v1 migreras: nya fält får standardvärden). false om storlek/version/innehåll är fel.
bool settingsFromBlob(Settings& out, const void* blob, size_t n, uint16_t ver);
