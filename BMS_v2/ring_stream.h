#pragma once
// Skickar en ringbuffert som bytes i bitar (webbsvar med känd längd) utan att kopiera hela bufferten.
// Ström: 4 byte antal (little endian), posterna äldst först och eventuellt en extra post sist.
// Ren kod (ingen Arduino).
#include <stddef.h>
#include <stdint.h>

static const uint16_t RING_EXTRA_MAX = 80;

// Läsvy av en ring (HistRing eller CellLogRing). total = antal poster som någonsin lagts till.
struct RingView {
  const uint8_t* buf;
  uint16_t cap;
  uint16_t head;
  uint16_t count;
  uint32_t total;
  uint16_t recSize;
};

// Ögonblicksbild vid svarets början: vilka poster som ska skickas.
struct RingSnap {
  uint32_t firstSeq;   // löpnummer (som total) för äldsta posten
  uint16_t count;
  uint16_t recSize;
  bool     hasExtra;
  uint8_t  extra[RING_EXTRA_MAX];
};

// extra (kan vara nullptr) måste vara recSize byte och högst RING_EXTRA_MAX.
RingSnap ringSnapTake(const RingView& v, const uint8_t* extra, size_t extraLen);
size_t   ringSnapSize(const RingSnap& s);
// Fyller out med strömmens bytes från index. Poster som skrivits över sedan ögonblicksbilden ersätts
// med den äldsta som finns kvar, så att tiden aldrig går baklänges. Returnerar 0 vid slutet.
size_t   ringSnapRead(const RingSnap& s, const RingView& now, size_t index, uint8_t* out, size_t maxLen);
