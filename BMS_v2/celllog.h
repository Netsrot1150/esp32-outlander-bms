#pragma once
// Cellogg: timmedel för varje cell i 14 dygn, för att se vilka celler som driver iväg.
// Ren kod (ingen Arduino). Tidsstämplar fungerar som i history.h (bit 31 = relativ upptid).
#include <stdint.h>
#include "cmu.h"
#include "history.h"

static const uint8_t  CELLLOG_CELLS = 64;      // 8 moduler * 8 celler; index = modulindex * 8 + cell
static const uint16_t CELLLOG_N = 336;         // 14 dygn med 1 post per timme
static const uint32_t CELLLOG_PERIOD_MS = 3600000;
static const uint8_t  CELLLOG_MISSING = 0xFF;  // cellen saknas eller var offline hela timmen
static const uint8_t  CELLLOG_MAX_OFF = 0xFE;  // större avvikelser begränsas hit

#pragma pack(push, 1)
struct CellLogRec {
  uint32_t t;
  uint16_t basemV;              // lägsta timmedel bland cellerna
  uint8_t  n;                   // antal cellplatser som används (högsta index + 1)
  uint8_t  reserved;
  uint8_t  off[CELLLOG_CELLS];  // timmedel - basemV i mV
};
#pragma pack(pop)
static_assert(sizeof(CellLogRec) == 72, "CellLogRec ska vara 72 byte");

struct CellLogRing {
  CellLogRec* buf;
  uint16_t    cap;
  uint16_t    head;
  uint16_t    count;
  uint32_t    total;   // antal poster som någonsin lagts till (för ring_stream)
};

void clInit(CellLogRing& r, CellLogRec* storage, uint16_t cap);
void clPush(CellLogRing& r, const CellLogRec& rec);
const CellLogRec& clAt(const CellLogRing& r, uint16_t i);            // i = 0 är äldst
void clLoad(CellLogRing& r, const CellLogRec* recs, uint16_t n);     // hoppar över relativa tider
void clFixRelative(CellLogRing& r, uint32_t nowEpoch, uint32_t nowUptimeS);

struct CellAccum {
  uint32_t sum[CELLLOG_CELLS];
  uint16_t n[CELLLOG_CELLS];
};
void cellAccumReset(CellAccum& a);
// Lägger till de utjämnade cellspänningarna för CMU:er som är online (anropas en gång per sekund).
void cellAccumAdd(CellAccum& a, const PackState& ps, const PackStats& st, uint32_t cmuTimeoutMs, uint32_t nowMs);
bool cellAccumMake(const CellAccum& a, uint32_t t, CellLogRec& out);   // false om inga värden
