#include "history_store.h"
#include <LittleFS.h>

static const uint32_t HIST_MAGIC = 0x31534948;   // "HIS1"
static const uint32_t CELL_MAGIC = 0x314C4543;   // "CEL1"
static bool s_ok = false;

bool histStoreBegin() {
  s_ok = LittleFS.begin(true, "/littlefs", 4, "spiffs");
  return s_ok;
}

// Filformat: magic, antal poster, posterna (äldst först). load lägger varje post i ringen via add.
template <typename Rec, typename Add>
static bool loadFile(const char* path, uint32_t magic, uint32_t maxN, Add add) {
  if (!s_ok || !LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  uint32_t hdr[2] = {0, 0};
  const bool ok = f.read((uint8_t*)hdr, sizeof(hdr)) == sizeof(hdr) && hdr[0] == magic && hdr[1] <= maxN;
  if (ok) {
    Rec rec;
    for (uint32_t i = 0; i < hdr[1]; i++) {
      if (f.read((uint8_t*)&rec, sizeof(rec)) != sizeof(rec)) break;
      add(rec);
    }
  }
  f.close();
  return ok;
}

// Skriver till en tillfällig fil och byter namn, så att en avbruten skrivning inte förstör den gamla.
template <typename Rec, typename At>
static bool saveFile(const char* path, const char* tmp, uint32_t magic, uint16_t count, At at) {
  if (!s_ok) return false;
  File f = LittleFS.open(tmp, "w");
  if (!f) return false;
  const uint32_t hdr[2] = {magic, count};
  bool ok = f.write((const uint8_t*)hdr, sizeof(hdr)) == sizeof(hdr);
  for (uint16_t i = 0; ok && i < count; i++) ok = f.write((const uint8_t*)&at(i), sizeof(Rec)) == sizeof(Rec);
  f.close();
  if (!ok) return false;
  LittleFS.remove(path);
  return LittleFS.rename(tmp, path);
}

bool histStoreLoad(HistRing& r) {
  return loadFile<HistRec>("/hist.bin", HIST_MAGIC, HIST_24H_N, [&](const HistRec& rec) { histLoad(r, &rec, 1); });
}

bool histStoreSave(const HistRing& r) {
  return saveFile<HistRec>("/hist.bin", "/hist.tmp", HIST_MAGIC, r.count,
                           [&](uint16_t i) -> const HistRec& { return histAt(r, i); });
}

bool cellLogStoreLoad(CellLogRing& r) {
  return loadFile<CellLogRec>("/cells.bin", CELL_MAGIC, CELLLOG_N, [&](const CellLogRec& rec) { clLoad(r, &rec, 1); });
}

bool cellLogStoreSave(const CellLogRing& r) {
  return saveFile<CellLogRec>("/cells.bin", "/cells.tmp", CELL_MAGIC, r.count,
                              [&](uint16_t i) -> const CellLogRec& { return clAt(r, i); });
}
