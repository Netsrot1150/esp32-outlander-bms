#include "ring_stream.h"
#include <string.h>

RingSnap ringSnapTake(const RingView& v, const uint8_t* extra, size_t extraLen) {
  RingSnap s;
  memset(&s, 0, sizeof(s));
  s.firstSeq = v.total - v.count;
  s.count = v.count;
  s.recSize = v.recSize;
  if (extra && extraLen == v.recSize && extraLen <= RING_EXTRA_MAX) {
    s.hasExtra = true;
    memcpy(s.extra, extra, extraLen);
  }
  return s;
}

size_t ringSnapSize(const RingSnap& s) { return 4 + ((size_t)s.count + (s.hasExtra ? 1 : 0)) * s.recSize; }

// Pekare till post nr i i ögonblicksbilden, läst ur ringen som den ser ut nu.
static const uint8_t* recPtr(const RingSnap& s, const RingView& now, uint16_t i) {
  uint32_t seq = s.firstSeq + i;
  const uint32_t oldest = now.total - now.count;
  if (seq < oldest) seq = oldest;
  const uint32_t back = now.total - seq;   // 1..count
  return now.buf + (size_t)((now.head + now.cap - back) % now.cap) * s.recSize;
}

size_t ringSnapRead(const RingSnap& s, const RingView& now, size_t index, uint8_t* out, size_t maxLen) {
  const size_t size = ringSnapSize(s);
  size_t done = 0;
  while (done < maxLen && index < size) {
    size_t n;
    if (index < 4) {
      const uint32_t cnt = s.count + (s.hasExtra ? 1 : 0);
      uint8_t hdr[4];
      memcpy(hdr, &cnt, 4);
      n = 4 - index;
      if (n > maxLen - done) n = maxLen - done;
      memcpy(out + done, hdr + index, n);
    } else {
      const size_t rec = (index - 4) / s.recSize;
      const size_t off = (index - 4) % s.recSize;
      const uint8_t* src = rec < s.count ? recPtr(s, now, (uint16_t)rec) : s.extra;
      n = s.recSize - off;
      if (n > maxLen - done) n = maxLen - done;
      memcpy(out + done, src + off, n);
    }
    done += n;
    index += n;
  }
  return done;
}
