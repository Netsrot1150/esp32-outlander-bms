#pragma once
// Simulerade CMU:er för test utan batteri (SIMULATE_CMUS). Ren kod (ingen Arduino).
#include <stdint.h>

typedef void (*SimEmit)(uint16_t id, const uint8_t data[8], void* ctx);

void simReset();
// Ett pollsvar: skickar 0x6X1–0x6X3 för CMU 1–8 via emit. Med mode == 1 bleeds celler över målet
// (0,5 mV/s – snabbare än verkligheten så att det syns i UI:t).
void simStep(uint16_t targetmV, uint8_t mode, uint32_t nowMs, SimEmit emit, void* ctx);
