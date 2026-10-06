#include "sim.h"

static const uint8_t SIM_CMUS = 8;
static float    s_cell[SIM_CMUS][8];
static uint32_t s_lastMs = 0;
static bool     s_init = false;
static uint32_t s_rng = 12345;

static float noise() {   // -0.5 .. 0.5
  s_rng = s_rng * 1103515245u + 12345u;
  return (float)((s_rng >> 16) & 0x7FFF) / 32767.0f - 0.5f;
}

void simReset() {
  s_init = false;
  s_rng = 12345;
}

static void init(uint32_t nowMs) {
  for (uint8_t m = 0; m < SIM_CMUS; m++)
    for (uint8_t k = 0; k < 8; k++)
      s_cell[m][k] = 3915.0f + (float)(((m * 8 + k) * 7) % 23) - 6.0f;   // 3909..3931 mV
  s_cell[2][4] = 3955.0f;   // en hög cell så att balanseringen syns
  s_lastMs = nowMs;
  s_init = true;
}

void simStep(uint16_t targetmV, uint8_t mode, uint32_t nowMs, SimEmit emit, void* ctx) {
  if (!s_init) init(nowMs);
  const float dt = (float)(nowMs - s_lastMs) / 1000.0f;
  s_lastMs = nowMs;

  for (uint8_t m = 0; m < SIM_CMUS; m++) {
    uint8_t bal = 0;
    for (uint8_t k = 0; k < 8; k++) {
      if (mode == 1 && s_cell[m][k] > (float)targetmV) {
        bal |= (uint8_t)(1u << k);
        s_cell[m][k] -= 0.5f * dt;
      }
    }
    const uint16_t base = (uint16_t)(0x600 | ((m + 1) << 4));
    uint8_t d[8];
    d[0] = bal;
    d[1] = 0x30;
    for (uint8_t t = 0; t < 5; t++) d[2 + t] = (uint8_t)(60 + ((m + t) % 3));   // 20-22 °C med offset 40
    d[7] = 0xFF;
    emit(base | 1, d, ctx);
    for (uint8_t half = 0; half < 2; half++) {
      for (uint8_t i = 0; i < 4; i++) {
        const uint16_t v = (uint16_t)(s_cell[m][half * 4 + i] + noise() * 2.0f);
        d[2 * i] = (uint8_t)(v >> 8);
        d[2 * i + 1] = (uint8_t)(v & 0xFF);
      }
      emit((uint16_t)(base | (2 + half)), d, ctx);
    }
  }
}
