#include "web.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include "app_state.h"
#include "config.h"
#include "settings_json.h"
#include "web_ui.h"
#include "clock.h"
#include "ring_stream.h"

static AsyncWebServer s_server(80);

static const char* levelName(uint8_t lv) { return lv == LV_ALARM ? "ALARM" : "WARN"; }

static double r1(double v) { return round(v * 10.0) / 10.0; }
static double r2(double v) { return round(v * 100.0) / 100.0; }

static RingView histView(const HistRing& h) {
  return RingView{(const uint8_t*)h.buf, h.cap, h.head, h.count, h.total, sizeof(HistRec)};
}

static RingView cellLogView() {
  return RingView{(const uint8_t*)g_cellLog.buf, g_cellLog.cap, g_cellLog.head, g_cellLog.count, g_cellLog.total,
                  sizeof(CellLogRec)};
}

// Skickar en ögonblicksbild av en ring. view() läses under AppLock för varje bit.
template <typename V>
static void sendRing(AsyncWebServerRequest* req, const RingSnap& snap, V view) {
  AsyncWebServerResponse* r = req->beginResponse(
      "application/octet-stream", ringSnapSize(snap), [snap, view](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
        AppLock lock;
        return ringSnapRead(snap, view(), index, buf, maxLen);
      });
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
}

static void locStr(char* out, size_t n, CellLoc l) {
  snprintf(out, n, "M%uC%u", (unsigned)(l.module + 1), (unsigned)(l.cell + 1));
}

// Bygger JSON och skickar den. Allt läses och serialiseras under AppLock.
template <typename F>
static void respondJson(AsyncWebServerRequest* req, F build, const char* attachment = nullptr) {
  AsyncResponseStream* r = req->beginResponseStream("application/json");
  r->addHeader("Cache-Control", "no-store");
  if (attachment) {
    char cd[64];
    snprintf(cd, sizeof(cd), "attachment; filename=%s", attachment);
    r->addHeader("Content-Disposition", cd);
  }
  {
    JsonDocument doc;
    AppLock lock;
    build(doc.to<JsonObject>());
    serializeJson(doc, *r);
  }
  req->send(r);
}

static void buildState(JsonObject o, const AppSnapshot& sn, const Settings& s) {
  const PackStats& st = sn.st;
  char buf[12];
  o["up"] = sn.nowMs / 1000;

  JsonObject p = o["pack"].to<JsonObject>();
  p["mV"] = st.packmV;
  p["cells"] = st.cellCount;
  p["minmV"] = st.minCellmV;
  locStr(buf, sizeof(buf), st.minLoc);
  p["minLoc"] = buf;
  JsonArray mi = p["minIdx"].to<JsonArray>();
  mi.add(st.minLoc.module);
  mi.add(st.minLoc.cell);
  p["maxmV"] = st.maxCellmV;
  locStr(buf, sizeof(buf), st.maxLoc);
  p["maxLoc"] = buf;
  JsonArray ma = p["maxIdx"].to<JsonArray>();
  ma.add(st.maxLoc.module);
  ma.add(st.maxLoc.cell);
  p["deltamV"] = st.deltamV;
  if (st.minTempC == TEMP_INVALID) {
    p["minT"] = nullptr;
    p["maxT"] = nullptr;
  } else {
    p["minT"] = st.minTempC;
    p["maxT"] = st.maxTempC;
  }
  p["online"] = st.online;
  p["expected"] = s.expectedCmus;

  JsonObject b = o["bal"].to<JsonObject>();
  b["mode"] = s.balMode == BAL_AUTO ? "auto" : (s.balMode == BAL_MANUAL ? "manual" : "off");
  b["state"] = balStateName(sn.bal.state);
  b["targetmV"] = sn.bal.targetmV;
  b["cmdMode"] = sn.bal.mode;
  b["reason"] = sn.bal.reason;

  JsonObject a = o["alarms"].to<JsonObject>();
  a["alarm"] = alarmBits(sn.al);
  a["warn"] = warnBits(sn.al);
  JsonArray act = a["active"].to<JsonArray>();
  for (uint8_t id = 0; id < AL_COUNT; id++) {
    if (sn.al.level[id] == LV_NONE) continue;
    JsonObject e = act.add<JsonObject>();
    e["id"] = id;
    e["name"] = alarmName(id);
    e["level"] = levelName(sn.al.level[id]);
    e["value"] = sn.al.value[id];
    e["loc"] = sn.al.loc[id];
  }

  JsonArray cm = o["cmus"].to<JsonArray>();
  for (uint8_t i = 0; i < st.moduleCount; i++) {
    const uint8_t id = st.moduleIds[i];
    const CmuData& c = sn.ps.cmu[id];
    JsonObject e = cm.add<JsonObject>();
    e["id"] = id;
    e["online"] = cmuOnline(c, s.cmuTimeoutMs, sn.nowMs);
    e["age"] = sn.nowMs - c.lastRxMs;
    uint32_t sum = 0;
    JsonArray cells = e["cells"].to<JsonArray>();
    JsonArray bal = e["bal"].to<JsonArray>();
    JsonArray bleed = e["bleedS"].to<JsonArray>();
    for (uint8_t k = 0; k < c.cellCount; k++) {
      cells.add(c.cellmV[k]);
      sum += c.cellmV[k];
      bal.add((c.balBits >> k) & 1);
      bleed.add(sn.bleedS[id][k]);
    }
    e["mV"] = sum;
    e["cmuMv"] = c.moduleMeasuredmV;
    JsonArray tc = e["tC"].to<JsonArray>();
    JsonArray tr = e["tRaw"].to<JsonArray>();
    for (uint8_t t = 0; t < TEMPS_PER_CMU; t++) {
      const int16_t v = tempC(c.tempRaw[t], s.tempOffset);
      if (v == TEMP_INVALID) tc.add(nullptr);
      else tc.add(v);
      tr.add(c.tempRaw[t]);
    }
  }

  JsonObject so = o["soc"].to<JsonObject>();
  so["valid"] = sn.soc.valid;
  so["pct"] = r1(socEnergyPct(sn.soc.pct, s));   // energi
  so["chargePct"] = r1(sn.soc.pct);              // laddning (Ah)
  // Medelcellen (packspänning / antal celler) från vilospänningskurvan, som jämförelse med lägsta cellen.
  if (st.cellCount) so["avgPct"] = r1(socEnergyPct(socFromOcv((uint16_t)(st.packmV / st.cellCount), s), s));
  else so["avgPct"] = nullptr;
  so["mode"] = sn.soc.mode == SOC_AH ? "ah" : "ocv";
  so["kWhLeft"] = r2(socKwhLeft(sn.soc, s, st.cellCount));
  so["capAh"] = s.capacityAhX10 / 10.0;

  JsonObject cu = o["cur"].to<JsonObject>();
  cu["enabled"] = sn.cur.enabled;
  cu["A"] = r1(sn.cur.amps);
  cu["kW"] = r2(sn.cur.amps * st.packmV / 1e6);
  cu["mV"] = sn.curMv;
  cu["saturated"] = sn.cur.saturated;
  cu["ahIn"] = r2(sn.soc.ahIn);
  cu["ahOut"] = r2(sn.soc.ahOut);
  cu["kWhIn"] = r2(sn.soc.kWhIn);
  cu["kWhOut"] = r2(sn.soc.kWhOut);
  const int32_t ml = socMinutesLeft(sn.soc, s, sn.cur.enabled, sn.cur.amps);
  if (ml < 0) cu["minutesLeft"] = nullptr;
  else cu["minutesLeft"] = ml;

  JsonObject tm = o["time"].to<JsonObject>();
  tm["known"] = sn.timeKnown;
  tm["epoch"] = sn.epoch;
}

static void buildDiag(JsonObject o, const AppSnapshot& sn, const Settings& s) {
  o["fw"] = FW_VERSION;
  o["up"] = sn.nowMs / 1000;
  o["heap"] = sn.heapFree;
  o["minHeap"] = sn.heapMin;
  o["reset"] = g_resetReason;
  o["sim"] = sn.can.simulated;

  JsonObject c = o["can"].to<JsonObject>();
  c["ok"] = sn.can.ok;
  c["rx"] = sn.can.rxFrames;
  c["other"] = sn.can.rxOther;
  c["txOk"] = sn.can.txOk;
  c["txBusy"] = sn.can.txBusy;
  c["txAborts"] = sn.can.txAborts;
  c["overflows"] = sn.can.overflows;
  c["tec"] = sn.can.tec;
  c["rec"] = sn.can.rec;
  c["eflg"] = sn.can.eflg;
  c["pollMs"] = sn.can.pollMs;
  c["mcpStep"] = sn.can.mcpStep;
  c["initCanstat"] = sn.can.initCanstat;
  c["canstat"] = sn.can.canstat;
  c["canctrl"] = sn.can.canctrl;
  c["initTries"] = sn.can.initTries;
  c["reinits"] = sn.can.reinits;
  c["spiOk"] = sn.can.spiOk;
  if (sn.can.lastErrorMs) c["lastErrS"] = (sn.nowMs - sn.can.lastErrorMs) / 1000;
  else c["lastErrS"] = nullptr;

  JsonObject n = o["net"].to<JsonObject>();
  n["apSsid"] = s.apSsid;
  n["apIp"] = sn.net.apIp;
  n["apClients"] = sn.net.apClients;
  n["staSsid"] = s.staSsid;
  n["staConnected"] = sn.net.staConnected;
  n["staIp"] = sn.net.staIp;
  n["rssi"] = sn.net.rssi;
  n["channel"] = sn.net.channel;
  n["espnow"] = s.espnowEnabled;
  n["espnowSent"] = sn.espnowSent;

  JsonArray cm = o["cmus"].to<JsonArray>();
  for (uint8_t i = 0; i < sn.st.moduleCount; i++) {
    const uint8_t id = sn.st.moduleIds[i];
    const CmuData& d = sn.ps.cmu[id];
    JsonObject e = cm.add<JsonObject>();
    e["id"] = id;
    e["status"] = d.status;
    e["invalid"] = d.invalidCells;
    JsonArray subs = e["subs"].to<JsonArray>();
    for (uint8_t k = 0; k < CMU_SUBS; k++) {
      JsonObject x = subs.add<JsonObject>();
      x["sub"] = k + 1;
      if (d.frames[k]) {
        char hex[24];
        const uint8_t* r = d.raw[k];
        snprintf(hex, sizeof(hex), "%02X %02X %02X %02X %02X %02X %02X %02X", r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7]);
        x["hex"] = hex;
        x["age"] = sn.nowMs - d.lastSubRxMs[k];
      } else {
        x["hex"] = nullptr;
        x["age"] = nullptr;
      }
      x["fps"] = sn.fps[id][k];
      x["n"] = d.frames[k];
    }
  }

  JsonObject cap = o["capture"].to<JsonObject>();
  cap["running"] = canCaptureRunning();
  cap["frames"] = canCaptureCount();

  JsonArray log = o["alarmLog"].to<JsonArray>();
  for (uint8_t i = 0; i < sn.al.logCount; i++) {
    const AlarmEvent* ev = alarmLogAt(sn.al, i);
    JsonObject e = log.add<JsonObject>();
    e["t"] = ev->upS;
    e["id"] = ev->id;
    e["name"] = alarmName(ev->id);
    e["level"] = levelName(ev->level);
    e["active"] = ev->active;
    e["value"] = ev->value;
    e["loc"] = ev->loc;
  }

  JsonObject cu = o["cur"].to<JsonObject>();
  cu["enabled"] = sn.cur.enabled;
  cu["mV"] = sn.curMv;
  cu["zeromV"] = s.curZeromV;
  cu["saturated"] = sn.cur.saturated;
  JsonObject tm = o["time"].to<JsonObject>();
  tm["known"] = sn.timeKnown;
  tm["epoch"] = sn.epoch;
  JsonObject hs = o["hist"].to<JsonObject>();
  hs["n24"] = g_hist24.count;
  hs["nLive"] = g_histLive.count;
  hs["nCells"] = g_cellLog.count;
  hs["fsOk"] = sn.fsOk;
}

static void sendOk(AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); }

void webBegin() {
  s_server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html; charset=utf-8", (const uint8_t*)WEB_UI_HTML, sizeof(WEB_UI_HTML) - 1);
  });

  s_server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    respondJson(req, [](JsonObject o) { buildState(o, g_snap, g_settings); });
  });

  s_server.on("/api/diag", HTTP_GET, [](AsyncWebServerRequest* req) {
    respondJson(req, [](JsonObject o) { buildDiag(o, g_snap, g_settings); });
  });

  s_server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
    respondJson(req, [](JsonObject o) { settingsToJson(g_settings, o, true); });
  });

  AsyncCallbackJsonWebHandler* settingsPost = new AsyncCallbackJsonWebHandler(
      "/api/settings", [](AsyncWebServerRequest* req, JsonVariant& json) {
        JsonObjectConst in = json.as<JsonObjectConst>();
        if (in.isNull()) {
          req->send(400, "application/json", "{\"error\":\"JSON-objekt förväntades\",\"field\":null}");
          return;
        }
        Settings tmp;
        {
          AppLock lock;
          tmp = g_settings;
        }
        const char* err = settingsFromJson(tmp, in);
        if (err) {
          JsonDocument d;
          d["error"] = "Ogiltigt värde";
          d["field"] = err;
          String out;
          serializeJson(d, out);
          req->send(400, "application/json", out);
          return;
        }
        {
          AppLock lock;
          g_settings = tmp;
          g_settingsDirty = true;
        }
        respondJson(req, [](JsonObject o) { settingsToJson(g_settings, o, true); });
      });
  settingsPost->setMethod(HTTP_POST);
  s_server.addHandler(settingsPost);

  s_server.on("/api/bleed/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    g_bleedResetReq = true;
    sendOk(req);
  });

  s_server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
    g_restartAtMs = (millis() + 500) | 1;
    sendOk(req);
  });

  s_server.on("/api/capture", HTTP_POST, [](AsyncWebServerRequest* req) {
    long sec = req->hasParam("s") ? req->getParam("s")->value().toInt() : 10;
    if (sec < 1) sec = 1;
    if (sec > 30) sec = 30;
    if (!canCaptureStart((uint16_t)sec)) {
      req->send(507, "application/json", "{\"error\":\"Minne saknas\"}");
      return;
    }
    sendOk(req);
  });

  s_server.on("/api/capture.csv", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (canCaptureRunning()) {
      req->send(409, "text/plain; charset=utf-8", "Inspelning pågår");
      return;
    }
    if (canCaptureCount() == 0) {
      req->send(404, "text/plain; charset=utf-8", "Ingen inspelning");
      return;
    }
    AsyncWebServerResponse* r = req->beginResponse(
        "text/csv", canCaptureCsvSize(),
        [](uint8_t* buf, size_t maxLen, size_t index) -> size_t { return canCaptureCsvRead(buf, maxLen, index); });
    r->addHeader("Content-Disposition", "attachment; filename=bms-can.csv");
    req->send(r);
  });

  s_server.on("/api/report", HTTP_GET, [](AsyncWebServerRequest* req) {
    respondJson(
        req,
        [](JsonObject o) {
          buildState(o["state"].to<JsonObject>(), g_snap, g_settings);
          buildDiag(o["diag"].to<JsonObject>(), g_snap, g_settings);
          settingsToJson(g_settings, o["settings"].to<JsonObject>(), true);
        },
        "bms-rapport.json");
  });

  s_server.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest* req) {
        if (!Update.isFinished() || Update.hasError()) {
          String m = String("Uppdatering misslyckades: ") + Update.errorString();
          req->send(500, "text/plain; charset=utf-8", m);
          return;
        }
        req->send(200, "text/plain; charset=utf-8", "Uppdatering klar – BMS:en startar om");
        g_restartAtMs = (millis() + 1000) | 1;
      },
      [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
        if (index == 0) {
          if (Update.isRunning()) Update.abort();
          Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
        }
        if (len && !Update.hasError()) Update.write(data, len);
        if (final && !Update.hasError()) Update.end(true);
      });

  // Historik och cellogg skickas i bitar direkt ur ringbufferten (känd längd). Tidigare byggdes hela
  // svaret i minnet (upp till 24 kB) och kapades tyst när minnet inte räckte.
  s_server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest* req) {
    const bool live = req->hasParam("range") && req->getParam("range")->value() == "10m";
    RingSnap snap;
    {
      AppLock lock;
      snap = ringSnapTake(histView(live ? g_histLive : g_hist24), nullptr, 0);
    }
    sendRing(req, snap, [live]() { return histView(live ? g_histLive : g_hist24); });
  });

  // Celloggen: 4 byte antal, sedan CellLogRec (72 byte) äldst först. Sista posten är den pågående
  // timmen (medel hittills) om det finns värden.
  s_server.on("/api/cells", HTTP_GET, [](AsyncWebServerRequest* req) {
    RingSnap snap;
    {
      AppLock lock;
      CellLogRec cur;
      const bool hasCur = cellAccumMake(g_cellAcc, histStamp(g_snap.timeKnown, g_snap.epoch, g_snap.nowMs / 1000), cur);
      snap = ringSnapTake(cellLogView(), hasCur ? (const uint8_t*)&cur : nullptr, sizeof(cur));
    }
    sendRing(req, snap, cellLogView);
  });

  AsyncCallbackJsonWebHandler* timePost =
      new AsyncCallbackJsonWebHandler("/api/time", [](AsyncWebServerRequest* req, JsonVariant& json) {
        const uint32_t e = json["epoch"] | 0u;
        clockSetEpoch(e, false);
        char out[64];
        snprintf(out, sizeof(out), "{\"known\":%s,\"epoch\":%lu}", clockKnown() ? "true" : "false",
                 (unsigned long)clockEpoch());
        req->send(200, "application/json", out);
      });
  timePost->setMethod(HTTP_POST);
  s_server.addHandler(timePost);

  s_server.on("/api/current/zero", HTTP_POST, [](AsyncWebServerRequest* req) {
    uint16_t mv;
    bool ok;
    {
      AppLock lock;
      mv = g_snap.curMv;
      ok = g_settings.curEnabled && mv >= 500 && mv <= 2800;
      if (ok) {
        g_settings.curZeromV = mv;
        g_settingsDirty = true;
      }
    }
    if (!ok) {
      req->send(400, "application/json", "{\"error\":\"Ingen giltig sensorspänning – är strömsensorn aktiverad och ansluten?\"}");
      return;
    }
    char out[32];
    snprintf(out, sizeof(out), "{\"zeromV\":%u}", (unsigned)mv);
    req->send(200, "application/json", out);
  });

  s_server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain; charset=utf-8", "Hittades inte"); });
  s_server.begin();
}
