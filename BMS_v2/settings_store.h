#pragma once
// Läser och sparar inställningar och laddnivå i NVS (flash).
#include "settings.h"

void settingsLoad(Settings& s);        // standardvärden om inget giltigt finns sparat; v1 migreras; Manuell laddas som Av
void settingsSave(const Settings& s);

struct SocPersist {
  float  pct;
  double ahIn, ahOut, kWhIn, kWhOut;
};
bool socLoad(SocPersist& out);
void socSave(const SocPersist& in);
