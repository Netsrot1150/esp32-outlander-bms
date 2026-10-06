#pragma once
// Laddnivå (SoC): vilospänningskurva och Ah-räkning. Ren kod (ingen Arduino).
#include <stdint.h>
#include "cmu.h"
#include "settings.h"

enum SocMode : uint8_t { SOC_OCV = 0, SOC_AH = 1 };

static const uint32_t SOC_FULL_HOLD_MS = 60000;        // "laddaren klar" måste gälla så här länge
static const uint32_t SOC_REST_MS = 30UL * 60000UL;    // vila före OCV-korrigering
static const float    SOC_REST_A = 0.5f;
static const float    SOC_CHARGING_A = 2.0f;
static const float    SOC_OCV_TAU_MS = 60000.0f;       // utjämning utan strömsensor

struct SocState {
  float    pct;
  bool     valid;
  uint8_t  mode;               // SocMode
  bool     chargedSinceFull;   // laddström > 2 A sedan senaste full-korrigering
  uint32_t fullSinceMs;        // 0 = villkoret för full uppfylls inte
  uint32_t restSinceMs;        // 0 = ingen vila
  double   ahIn, ahOut, kWhIn, kWhOut;
};

// SoC-tillståndet (pct) är laddning (Ah), eftersom Ah-räkningen räknar laddning. Det som visas är
// energiprocenten: andel av energin mellan socEmptymV och socFullmV enligt cellens spänningskurva.
float ocvPct(uint16_t cellmV);                          // tabellen: 0–100 % för 2750–4100 mV (LEV46)
float socFromOcv(uint16_t cellmV, const Settings& s);   // omskalad till socEmptymV..socFullmV
float socEnergyPct(float socPct, const Settings& s);    // laddnings-% -> energi-% (samma gränser)
void  socInit(SocState& st, float savedPct);            // savedPct < 0 = inget sparat
void  socUpdate(SocState& st, const PackStats& ps, const Settings& s, bool haveCurrent, float amps,
                uint32_t dtMs, uint32_t nowMs);
float socKwhLeft(const SocState& st, const Settings& s, uint16_t cellCount);   // energi kvar ned till socEmptymV
int32_t socMinutesLeft(const SocState& st, const Settings& s, bool haveCurrent, float amps);   // -1 = okänt
