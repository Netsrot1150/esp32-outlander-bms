#pragma once
// Sparar 24 h-historiken och celloggen i LittleFS-partitionen "spiffs" (128 kB).
#include "history.h"
#include "celllog.h"

bool histStoreBegin();                     // monterar (formaterar vid första start)
bool histStoreLoad(HistRing& r);
bool histStoreSave(const HistRing& r);
bool cellLogStoreLoad(CellLogRing& r);
bool cellLogStoreSave(const CellLogRing& r);
