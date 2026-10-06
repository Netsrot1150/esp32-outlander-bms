#include "soc.h"
#include <math.h>
#include <string.h>

// Vilospänning per cell mot laddning (% av kapaciteten mellan 2,75 och 4,10 V) för GS Yuasa LEV46
// (LMO/NMC-blandning, samma kemi som LEV40). Sammanställd ur:
//  - LEV46-datablad: 2,75–4,10 V, 46 Ah, nominellt 3,75 V (kurvans medel-OCV blir 3,83 V)
//  - Outlander-BMU:ns vilovärden: 3,80–3,82 V ≈ 25–30 %, 4,05 V ≈ 90 %, 4,10 V = 100 %
//  - urladdningslogg från Outlander: knä vid 3,45–3,60 V kring 10–15 %, brant fall under 3,3 V
// Kalibreras mot egen mätning när strömsensorn är på plats.
static const uint16_t OCV_MV[] = {2750, 3000, 3300, 3450, 3550, 3630, 3700, 3760, 3800, 3850, 3900, 3950, 4000, 4050, 4100};
static const float    OCV_PCT[] = {0, 1, 3, 7, 11, 15, 20, 25, 30, 42, 53, 65, 77, 88, 100};
static const int      OCV_N = sizeof(OCV_MV) / sizeof(OCV_MV[0]);

static float clampPct(float p) { return p < 0.0f ? 0.0f : (p > 100.0f ? 100.0f : p); }

float ocvPct(uint16_t mv) {
  if (mv <= OCV_MV[0]) return 0.0f;
  if (mv >= OCV_MV[OCV_N - 1]) return 100.0f;
  for (int i = 1; i < OCV_N; i++) {
    if (mv <= OCV_MV[i]) {
      const float f = (float)(mv - OCV_MV[i - 1]) / (float)(OCV_MV[i] - OCV_MV[i - 1]);
      return OCV_PCT[i - 1] + f * (OCV_PCT[i] - OCV_PCT[i - 1]);
    }
  }
  return 100.0f;
}

// Spänning (mV) vid laddning q (% enligt tabellen). Omvänd tabellslagning.
static float mvAtPct(float q) {
  if (q <= OCV_PCT[0]) return OCV_MV[0];
  for (int i = 1; i < OCV_N; i++) {
    if (q <= OCV_PCT[i]) {
      const float f = (q - OCV_PCT[i - 1]) / (OCV_PCT[i] - OCV_PCT[i - 1]);
      return OCV_MV[i - 1] + f * (float)(OCV_MV[i] - OCV_MV[i - 1]);
    }
  }
  return OCV_MV[OCV_N - 1];
}

// Energi från 0 % upp till laddningen q, i %·mV (∫ V dq). Exakt för den styckvis linjära kurvan.
static float energyAt(float q) {
  float e = 0.0f;
  for (int i = 1; i < OCV_N && q > OCV_PCT[i - 1]; i++) {
    const float hi = q < OCV_PCT[i] ? q : OCV_PCT[i];
    e += (hi - OCV_PCT[i - 1]) * (OCV_MV[i - 1] + mvAtPct(hi)) * 0.5f;
  }
  return e;
}

// Laddningen (tabellens %) vid socPct mellan socEmptymV och socFullmV.
static float tablePct(float socPct, const Settings& s) {
  const float lo = ocvPct(s.socEmptymV), hi = ocvPct(s.socFullmV);
  return lo + clampPct(socPct) / 100.0f * (hi - lo);
}

float socEnergyPct(float socPct, const Settings& s) {
  const float e0 = energyAt(ocvPct(s.socEmptymV)), e1 = energyAt(ocvPct(s.socFullmV));
  if (e1 - e0 < 1.0f) return 0.0f;
  return clampPct((energyAt(tablePct(socPct, s)) - e0) / (e1 - e0) * 100.0f);
}

float socFromOcv(uint16_t mv, const Settings& s) {
  const float lo = ocvPct(s.socEmptymV), hi = ocvPct(s.socFullmV);
  if (hi - lo < 1.0f) return 0.0f;
  return clampPct((ocvPct(mv) - lo) / (hi - lo) * 100.0f);
}

void socInit(SocState& st, float savedPct) {
  memset(&st, 0, sizeof(st));
  if (savedPct >= 0.0f) {
    st.pct = clampPct(savedPct);
    st.valid = true;
  }
}

void socUpdate(SocState& st, const PackStats& ps, const Settings& s, bool haveCurrent, float amps,
               uint32_t dtMs, uint32_t nowMs) {
  if (!ps.hasCells) return;
  const float ocv = socFromOcv(ps.minCellmV, s);

  if (!haveCurrent) {
    st.mode = SOC_OCV;
    if (!st.valid) {
      st.pct = ocv;
      st.valid = true;
      return;
    }
    const float a = (float)dtMs / (SOC_OCV_TAU_MS + (float)dtMs);
    st.pct = clampPct(st.pct + (ocv - st.pct) * a);
    return;
  }

  st.mode = SOC_AH;
  if (!st.valid) {
    st.pct = ocv;
    st.valid = true;
  }
  const double h = dtMs / 3600000.0;
  const double ah = (double)amps * h;
  const double kwh = ah * (ps.packmV / 1000.0) / 1000.0;
  if (ah >= 0) {
    st.ahIn += ah;
    st.kWhIn += kwh;
  } else {
    st.ahOut -= ah;
    st.kWhOut -= kwh;
  }
  const float capAh = s.capacityAhX10 / 10.0f;
  if (capAh > 0.0f) st.pct = clampPct(st.pct + (float)(ah / capAh * 100.0));

  // Full: laddaren har slutat efter en laddning
  if (amps > SOC_CHARGING_A) st.chargedSinceFull = true;
  const bool fullNow = st.chargedSinceFull && ps.maxCellmV + 20 >= s.socFullmV && amps > -SOC_REST_A && amps < 1.0f;
  if (fullNow) {
    if (st.fullSinceMs == 0) {
      st.fullSinceMs = nowMs | 1;
    } else if (nowMs - st.fullSinceMs >= SOC_FULL_HOLD_MS) {
      st.pct = 100.0f;
      st.chargedSinceFull = false;
      st.fullSinceMs = 0;
    }
  } else {
    st.fullSinceMs = 0;
  }

  // Vila: korrigera mot vilospänningen (tar bort nollpunktsdrift)
  if (fabsf(amps) < SOC_REST_A) {
    if (st.restSinceMs == 0) {
      st.restSinceMs = nowMs | 1;
    } else if (nowMs - st.restSinceMs >= SOC_REST_MS) {
      st.pct = ocv;
      st.restSinceMs = nowMs | 1;
    }
  } else {
    st.restSinceMs = 0;
  }
}

float socKwhLeft(const SocState& st, const Settings& s, uint16_t cellCount) {
  const float e = energyAt(tablePct(st.pct, s)) - energyAt(ocvPct(s.socEmptymV));   // %·mV per cell
  return e / 100.0f * (s.capacityAhX10 / 10.0f) * cellCount / 1e6f;
}

int32_t socMinutesLeft(const SocState& st, const Settings& s, bool haveCurrent, float amps) {
  if (!haveCurrent || !st.valid || fabsf(amps) < 1.0f) return -1;
  const float capAh = s.capacityAhX10 / 10.0f;
  const float ah = amps < 0 ? st.pct / 100.0f * capAh : (100.0f - st.pct) / 100.0f * capAh;
  return (int32_t)(ah / fabsf(amps) * 60.0f);
}
