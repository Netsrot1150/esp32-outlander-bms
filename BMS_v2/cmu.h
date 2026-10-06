#pragma once
// Datamodell och avkodning för Mitsubishi Outlander PHEV-CMU:er. Ren kod (ingen Arduino).
#include <stdint.h>
#include "config.h"

struct CmuData {
  bool     seen;
  uint32_t lastRxMs;
  uint8_t  cellCount;                  // 4 eller 8 (0 innan spänningar kommit)
  uint16_t cellmV[CELLS_PER_CMU];      // utjämnat värde (avrundat), 0 = inget giltigt värde ännu
  uint16_t cellRawmV[CELLS_PER_CMU];   // senast mottagna värde
  uint32_t cellQ[CELLS_PER_CMU];       // utjämnat värde i mV << 16
  uint8_t  tempRaw[TEMPS_PER_CMU];     // 0xFF = ingen givare
  uint8_t  balBits;                    // bit k = cell k+1 bleeds
  uint8_t  status;                     // byte 1 i 0x6X1, okänd betydelse
  uint8_t  raw[CMU_SUBS][8];           // senaste rådata för sub 1-5
  uint32_t lastSubRxMs[CMU_SUBS];
  uint32_t frames[CMU_SUBS];
  uint16_t moduleMeasuredmV;           // CMU:ns egen mätning av modulspänningen (0x6X5 byte 6-7), 0 = okänd
  uint32_t invalidCells;
};

struct PackState {
  CmuData cmu[MAX_CMU_ID + 1];         // index = CMU-id, [0] används inte
};

struct CellLoc {
  uint8_t module;                      // index i modullistan (0 = lägsta CMU-id)
  uint8_t cell;                        // 0..7
};

struct PackStats {
  uint8_t  moduleCount;                // antal CMU:er som någon gång hörts
  uint8_t  moduleIds[MAX_CMU_ID];      // deras id i stigande ordning
  uint8_t  online;
  uint16_t cellCount;                  // giltiga celler i CMU:er som är online
  uint32_t packmV;
  uint16_t minCellmV, maxCellmV;
  CellLoc  minLoc, maxLoc;
  uint16_t deltamV;
  int16_t  minTempC, maxTempC;         // TEMP_INVALID om ingen giltig givare
  uint8_t  minTempMod, maxTempMod;
  bool     hasCells;
};

static const int16_t TEMP_INVALID = -128;

void packInit(PackState& ps);
// Avkodar en CAN-ram. Returnerar true om det var en CMU-ram (0x6X1..0x6X5, 8 byte).
bool decodeFrame(PackState& ps, uint32_t id, const uint8_t* d, uint8_t len, uint32_t nowMs);
// T [°C] = rå - offset. TEMP_INVALID om rå = 0xFF.
int16_t tempC(uint8_t raw, int16_t offset);
bool cmuOnline(const CmuData& c, uint32_t timeoutMs, uint32_t nowMs);
// Statistik över CMU:er som är online. Med prev behålls förra min/max-platsen så länge den cellen
// ligger inom STICKY_LOC_Q av det nya extremvärdet, så att platsen inte hoppar mellan lika celler.
void computeStats(const PackState& ps, int16_t tempOffset, uint32_t cmuTimeoutMs, uint32_t nowMs, PackStats& out,
                  const PackStats* prev = nullptr);
