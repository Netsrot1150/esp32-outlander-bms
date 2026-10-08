#pragma once
// Historikposter, ringbuffertar och minutmedel. Ren kod (ingen Arduino).
#include <stdint.h>
#include "cmu.h"

#pragma pack(push, 1)
struct HistRec {
  uint32_t t;            // epoch-sekunder; bit 31 satt = upptid i s från denna start (tiden okänd)
  uint16_t packdV;       // 0,1 V
  uint16_t minCellmV;
  uint16_t maxCellmV;
  int8_t   minT;         // °C, HIST_NO_TEMP = okänd
  int8_t   maxT;
  int16_t  currentdA;    // 0,1 A, HIST_NO_CURRENT = ingen sensor
  uint16_t socX10;       // 0,1 %
};
#pragma pack(pop)
static_assert(sizeof(HistRec) == 16, "HistRec ska vara 16 byte");

static const uint32_t HIST_REL_FLAG = 0x80000000u;
static const int8_t   HIST_NO_TEMP = INT8_MIN;
static const int16_t  HIST_NO_CURRENT = INT16_MIN;
static const uint16_t HIST_24H_N = 1440;   // 1 post per minut
static const uint16_t HIST_LIVE_N = 600;   // 1 post per sekund

struct HistRing {
  HistRec* buf;
  uint16_t cap;
  uint16_t head;    // nästa skrivposition
  uint16_t count;
  uint32_t total;   // antal poster som någonsin lagts till (för ring_stream)
};

void histInit(HistRing& r, HistRec* storage, uint16_t cap);
void histPush(HistRing& r, const HistRec& rec);
const HistRec& histAt(const HistRing& r, uint16_t i);                 // i = 0 är äldst
void histLoad(HistRing& r, const HistRec* recs, uint16_t n);          // hoppar över relativa tider
void histFixRelative(HistRing& r, uint32_t nowEpoch, uint32_t nowUptimeS);
uint32_t histStamp(bool clockKnown, uint32_t epoch, uint32_t uptimeS);

struct HistAccum {
  uint32_t n, nCur, nTemp;
  double   packmV, minmV, maxmV, socPct, amps;
  int16_t  minT, maxT;
};
void accumReset(HistAccum& a);
void accumAdd(HistAccum& a, const PackStats& st, bool haveCurrent, float amps, float socPct);
bool accumMake(const HistAccum& a, uint32_t t, HistRec& out);   // false om inga värden
HistRec histNow(const PackStats& st, bool haveCurrent, float amps, float socPct, uint32_t t);
