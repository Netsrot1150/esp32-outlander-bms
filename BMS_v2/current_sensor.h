#pragma once
// Läser WCS1700:s analoga utsignal på GPIO3. I simulering: syntetisk ström.
#include <stdint.h>

void currentSensorBegin();
uint16_t currentSensorReadMv(uint16_t zeromV, uint16_t sensX100);   // medel av 64 läsningar
