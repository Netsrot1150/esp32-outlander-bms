#include "test_framework.h"
#include "ring_stream.h"
#include "history.h"
#include <cstring>
#include <vector>

static HistRec rec(uint32_t t) { HistRec r; std::memset(&r, 0, sizeof(r)); r.t = t; return r; }

static RingView view(const HistRing& r) {
  return RingView{(const uint8_t*)r.buf, r.cap, r.head, r.count, r.total, sizeof(HistRec)};
}

// Läser hela strömmen i bitar om `chunk` byte, som webbservern gör.
static std::vector<uint8_t> readAll(const RingSnap& s, const HistRing& r, size_t chunk) {
  std::vector<uint8_t> out(ringSnapSize(s));
  size_t idx = 0;
  while (idx < out.size()) {
    const size_t n = ringSnapRead(s, view(r), idx, out.data() + idx, chunk < out.size() - idx ? chunk : out.size() - idx);
    if (n == 0) break;
    idx += n;
  }
  out.resize(idx);
  return out;
}

static uint32_t u32(const std::vector<uint8_t>& b, size_t at) { uint32_t v; std::memcpy(&v, b.data() + at, 4); return v; }

TEST(ring_stream_hel_ring_i_sma_bitar) {
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  for (uint32_t i = 1; i <= 6; i++) histPush(r, rec(i));
  CHECK_EQ(r.total, 6u);
  const RingSnap s = ringSnapTake(view(r), nullptr, 0);
  CHECK_EQ(ringSnapSize(s), 4u + 4u * 16u);
  const std::vector<uint8_t> b = readAll(s, r, 7);   // udda bitstorlek korsar postgränser
  CHECK_EQ(b.size(), 4u + 4u * 16u);
  CHECK_EQ(u32(b, 0), 4u);
  CHECK_EQ(u32(b, 4), 3u);
  CHECK_EQ(u32(b, 4 + 3 * 16), 6u);
}

TEST(ring_stream_tom_ring) {
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  const RingSnap s = ringSnapTake(view(r), nullptr, 0);
  const std::vector<uint8_t> b = readAll(s, r, 100);
  CHECK_EQ(b.size(), 4u);
  CHECK_EQ(u32(b, 0), 0u);
}

TEST(ring_stream_extra_post_sist) {
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  histPush(r, rec(10)); histPush(r, rec(20));
  const HistRec cur = rec(99);
  const RingSnap s = ringSnapTake(view(r), (const uint8_t*)&cur, sizeof(cur));
  const std::vector<uint8_t> b = readAll(s, r, 5);
  CHECK_EQ(b.size(), 4u + 3u * 16u);
  CHECK_EQ(u32(b, 0), 3u);
  CHECK_EQ(u32(b, 4 + 1 * 16), 20u);
  CHECK_EQ(u32(b, 4 + 2 * 16), 99u);
}

// Nya poster under överföringen: redan skickade poster påverkas inte, och överskrivna äldsta poster
// ersätts med den äldsta som finns kvar så att tiden aldrig går baklänges.
TEST(ring_stream_nya_poster_under_overforing) {
  HistRec store[4]; HistRing r; histInit(r, store, 4);
  for (uint32_t i = 1; i <= 4; i++) histPush(r, rec(i));
  const RingSnap s = ringSnapTake(view(r), nullptr, 0);
  std::vector<uint8_t> b(ringSnapSize(s));
  size_t idx = ringSnapRead(s, view(r), 0, b.data(), 4 + 16);   // antal + post 1
  histPush(r, rec(5)); histPush(r, rec(6));                     // skriver över post 1 och 2
  while (idx < b.size()) idx += ringSnapRead(s, view(r), idx, b.data() + idx, b.size() - idx);
  CHECK_EQ(u32(b, 4 + 0 * 16), 1u);
  CHECK_EQ(u32(b, 4 + 1 * 16), 3u);   // post 2 är överskriven -> äldsta kvarvarande
  CHECK_EQ(u32(b, 4 + 2 * 16), 3u);
  CHECK_EQ(u32(b, 4 + 3 * 16), 4u);
}

TEST(ring_stream_slut_ger_noll) {
  HistRec store[2]; HistRing r; histInit(r, store, 2);
  histPush(r, rec(1));
  const RingSnap s = ringSnapTake(view(r), nullptr, 0);
  uint8_t buf[64];
  CHECK_EQ(ringSnapRead(s, view(r), ringSnapSize(s), buf, sizeof(buf)), 0u);
}
