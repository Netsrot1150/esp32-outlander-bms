#include "settings_store.h"
#include <Preferences.h>

static const char* NVS_NAMESPACE = "bms";

void settingsLoad(Settings& s) {
  settingsDefaults(s);
  Preferences p;
  if (p.begin(NVS_NAMESPACE, true)) {
    const uint16_t ver = p.getUShort("ver", 0);
    const size_t n = p.getBytesLength("cfg");
    if (n > 0 && n <= sizeof(Settings)) {
      uint8_t blob[sizeof(Settings)];
      p.getBytes("cfg", blob, n);
      Settings tmp;
      if (settingsFromBlob(tmp, blob, n, ver)) s = tmp;
    }
    p.end();
  }
  if (s.balMode == BAL_MANUAL) s.balMode = BAL_OFF;   // manuellt testläge överlever inte omstart
}

void settingsSave(const Settings& s) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return;
  p.putBytes("cfg", &s, sizeof(s));
  p.putUShort("ver", SETTINGS_VERSION);
  p.end();
}

bool socLoad(SocPersist& out) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, true)) return false;
  const bool ok = p.getBytesLength("socp") == sizeof(out) && p.getBytes("socp", &out, sizeof(out)) == sizeof(out);
  p.end();
  return ok && out.pct >= 0.0f && out.pct <= 100.0f;
}

void socSave(const SocPersist& in) {
  Preferences p;
  if (!p.begin(NVS_NAMESPACE, false)) return;
  p.putBytes("socp", &in, sizeof(in));
  p.end();
}
