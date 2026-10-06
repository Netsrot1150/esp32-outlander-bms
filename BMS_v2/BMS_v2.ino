// BMS v2 – ESP32-C3 med 8 Mitsubishi Outlander PHEV-CMU:er.
// Design: docs/design-v2.md och docs/design-v2.1-laddniva-strom-historik.md
#include <Arduino.h>
#include <esp_system.h>
#include <math.h>
#include <string.h>
#include "config.h"
#include "app_state.h"
#include "settings_store.h"
#include "packet_builder.h"
#include "espnow_tx.h"
#include "web.h"
#include "current_sensor.h"
#include "clock.h"
#include "history_store.h"

AppSnapshot       g_snap;
Settings          g_settings;
SemaphoreHandle_t g_appMutex = nullptr;
volatile bool     g_settingsDirty = false;
volatile bool     g_bleedResetReq = false;
volatile uint32_t g_restartAtMs = 0;
HistRing          g_hist24;
HistRing          g_histLive;
CellLogRing       g_cellLog;
CellAccum         g_cellAcc;
const char*       g_resetReason = "?";

static HistRec        s_buf24[HIST_24H_N];
static HistRec        s_bufLive[HIST_LIVE_N];
static CellLogRec     s_bufCells[CELLLOG_N];
static PackStats      s_st;
static SocState       s_soc;
static CurrentStatus  s_cur;
static uint16_t       s_curMv = 0;
static HistAccum      s_acc;
static bool           s_fsOk = false;
static bool           s_clockWasKnown = false;
static uint32_t       s_lastEvalMs = 0;
static float          s_savedSocPct = -1.0f;
static double         s_savedAh = 0.0;

static PackState      s_ps;
static CanCounters    s_can;
static BalancerMemory s_balMem;
static AlarmState     s_alarms;
static NetStatus      s_net;
static uint32_t       s_bleedS[MAX_CMU_ID + 1][CELLS_PER_CMU];
static uint32_t       s_lastFrames[MAX_CMU_ID + 1][CMU_SUBS];
static uint16_t       s_fps[MAX_CMU_ID + 1][CMU_SUBS];
static uint32_t       s_ovfHist[10];          // RX-overflow-räknaren de senaste 10 sekunderna
static uint8_t        s_ovfIdx = 0;
static const uint32_t OVF_WARN_PER_10S = 50;  // enstaka tappade ramar är ofarliga (25 Hz per CMU)
static const uint32_t CAN_WARN_HOLD_MS = 30000;   // varningen ligger kvar så att den inte fladdrar
static uint32_t       s_canErrMs = 0;             // senaste gången ett CAN-fel sågs, 0 = aldrig
static bool           s_espnowOn = false;
static uint8_t        s_seq = 0;

// Var 100:e ms: statistik, balanseringsbeslut, larm och ny snapshot till webben.
static void evaluate() {
  canSnapshot(s_ps, s_can);
  const uint32_t now = millis();   // efter kopian, så att inga mottagningstider ligger i framtiden
  Settings s;
  {
    AppLock lock;
    s = g_settings;
  }
  const PackStats prev = s_st;
  computeStats(s_ps, s.tempOffset, s.cmuTimeoutMs, now, s_st, &prev);
  const PackStats& st = s_st;
  const uint32_t dt = s_lastEvalMs ? now - s_lastEvalMs : 0;
  s_lastEvalMs = now;
  if (s.curEnabled) s_curMv = currentSensorReadMv(s.curZeromV, s.curSensX100);
  else s_curMv = 0;
  s_cur = currentFromMv(s_curMv, s);
  socUpdate(s_soc, st, s, s_cur.enabled, s_cur.amps, dt, now);
  const BalanceDecision d = balancerDecide(st, s, s_balMem, now);
  if (d.manualExpired) {
    AppLock lock;
    g_settings.balMode = BAL_OFF;
    g_settingsDirty = true;
  }
  canSetBalance(d.targetmV, d.mode);
  const uint32_t ovf10s = s_can.overflows - s_ovfHist[s_ovfIdx];   // äldsta värdet i ringbufferten
  if (!s_can.ok || (s_can.lastErrorMs != 0 && now - s_can.lastErrorMs < 10000) || ovf10s > OVF_WARN_PER_10S)
    s_canErrMs = now | 1;
  const bool canErr = s_canErrMs != 0 && now - s_canErrMs < CAN_WARN_HOLD_MS;
  alarmsEvaluate(s_alarms, st, s, canErr, now, &s_cur);

  AppLock lock;
  g_snap.nowMs = now;
  g_snap.ps = s_ps;
  g_snap.st = st;
  g_snap.bal = d;
  g_snap.al = s_alarms;
  g_snap.can = s_can;
  g_snap.net = s_net;
  memcpy(g_snap.bleedS, s_bleedS, sizeof(s_bleedS));
  memcpy(g_snap.fps, s_fps, sizeof(s_fps));
  g_snap.espnowSent = espnowSentCount();
  g_snap.heapFree = ESP.getFreeHeap();
  g_snap.heapMin = ESP.getMinFreeHeap();
  g_snap.soc = s_soc;
  g_snap.cur = s_cur;
  g_snap.curMv = s_curMv;
  g_snap.timeKnown = clockKnown();
  g_snap.epoch = clockEpoch();
  g_snap.fsOk = s_fsOk;
}

// Varje sekund: bleed-tid, ramar per sekund, nätverksstatus och återanslutning till hemnätet.
static void everySecond() {
  const uint32_t now = millis();
  Settings s;
  {
    AppLock lock;
    s = g_settings;
  }
  if (g_bleedResetReq) {
    memset(s_bleedS, 0, sizeof(s_bleedS));
    g_bleedResetReq = false;
  }
  for (uint8_t id = 1; id <= MAX_CMU_ID; id++) {
    const CmuData& c = s_ps.cmu[id];
    for (uint8_t k = 0; k < CMU_SUBS; k++) {
      s_fps[id][k] = (uint16_t)(c.frames[k] - s_lastFrames[id][k]);
      s_lastFrames[id][k] = c.frames[k];
    }
    if (!cmuOnline(c, s.cmuTimeoutMs, now)) continue;
    for (uint8_t k = 0; k < CELLS_PER_CMU; k++)
      if (c.balBits & (1u << k)) s_bleedS[id][k]++;
  }
  s_ovfHist[s_ovfIdx] = s_can.overflows;
  s_ovfIdx = (uint8_t)((s_ovfIdx + 1) % 10);
  clockLoop();
  const bool known = clockKnown();
  const float energyPct = socEnergyPct(s_soc.pct, s);   // historiken visar energiprocent
  const uint32_t upS = now / 1000;
  {
    AppLock lock;
    if (known && !s_clockWasKnown) {
      histFixRelative(g_hist24, clockEpoch(), upS);
      histFixRelative(g_histLive, clockEpoch(), upS);
      clFixRelative(g_cellLog, clockEpoch(), upS);
    }
    cellAccumAdd(g_cellAcc, s_ps, s_st, s.cmuTimeoutMs, now);
    if (s_st.hasCells)
      histPush(g_histLive, histNow(s_st, s_cur.enabled, s_cur.amps, energyPct, histStamp(known, clockEpoch(), upS)));
  }
  s_clockWasKnown = known;
  accumAdd(s_acc, s_st, s_cur.enabled, s_cur.amps, energyPct);
  netLoop(s, now);
  netStatus(s_net);
}

static void sendPacket() {
  BmsPacket p;
  {
    AppLock lock;
    const int16_t cdA = g_snap.cur.enabled ? (int16_t)lroundf(g_snap.cur.amps * 10.0f) : BMS_NO_CURRENT;
    buildBmsPacket(p, g_snap.ps, g_snap.st, g_snap.bal, alarmBits(g_snap.al), warnBits(g_snap.al),
                   g_settings.tempOffset, g_settings.cmuTimeoutMs, g_snap.nowMs, s_seq++,
                   (uint16_t)lroundf(socEnergyPct(g_snap.soc.pct, g_settings) * 10.0f), cdA, g_snap.soc.mode);
  }
  espnowSend(p);
}

static void applySettingsIfDirty() {
  if (!g_settingsDirty) return;
  Settings s;
  {
    AppLock lock;
    s = g_settings;
    g_settingsDirty = false;
  }
  settingsSave(s);
  canSetPollMs(s.canPollMs);   // nätverk och ESP-NOW gäller efter omstart
}

// BOOT-knappen (GPIO9) intryckt i 5 s medan BMS:en är igång: återställ nätverksinställningarna.
static void checkBootButton(uint32_t now) {
  static uint32_t pressedSince = 0;
  if (digitalRead(PIN_BOOT_BTN) != LOW) {
    pressedSince = 0;
    return;
  }
  if (pressedSince == 0) {
    pressedSince = now | 1;
    return;
  }
  if (now - pressedSince < 5000) return;
  Serial.println("BOOT hölls i 5 s: återställer nätverksinställningarna och startar om");
  Settings s;
  {
    AppLock lock;
    settingsDefaultsNetwork(g_settings);
    s = g_settings;
  }
  settingsSave(s);
  delay(200);
  ESP.restart();
}

// Varje minut: minutmedel till 24 h-historiken.
static void everyMinute() {
  HistRec r;
  if (accumMake(s_acc, histStamp(clockKnown(), clockEpoch(), millis() / 1000), r)) {
    AppLock lock;
    histPush(g_hist24, r);
  }
  accumReset(s_acc);
}

// Var 5:e minut: spara laddnivå och räknare om de ändrats.
static void saveSocIfChanged() {
  if (!s_soc.valid) return;
  const double ah = s_soc.ahIn + s_soc.ahOut;
  if (fabsf(s_soc.pct - s_savedSocPct) < 0.5f && fabs(ah - s_savedAh) < 0.5) return;
  const SocPersist sp = {s_soc.pct, s_soc.ahIn, s_soc.ahOut, s_soc.kWhIn, s_soc.kWhOut};
  socSave(sp);
  s_savedSocPct = s_soc.pct;
  s_savedAh = ah;
}

// Varje timme: timmedel för alla celler till celloggen, som sparas i flash direkt.
static void everyHour() {
  CellLogRec r;
  AppLock lock;
  if (cellAccumMake(g_cellAcc, histStamp(clockKnown(), clockEpoch(), millis() / 1000), r)) {
    clPush(g_cellLog, r);
    if (s_fsOk) cellLogStoreSave(g_cellLog);
  }
  cellAccumReset(g_cellAcc);
}

static const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "strömpåslag";
    case ESP_RST_EXT: return "reset-pinne";
    case ESP_RST_SW: return "omstart (programvara/OTA)";
    case ESP_RST_PANIC: return "krasch";
    case ESP_RST_INT_WDT: return "vakthund (avbrott)";
    case ESP_RST_TASK_WDT: return "vakthund (task)";
    case ESP_RST_WDT: return "vakthund";
    case ESP_RST_DEEPSLEEP: return "djupsömn";
    case ESP_RST_BROWNOUT: return "spänningsfall (brownout)";
    case ESP_RST_SDIO: return "SDIO";
    default: return "okänd";
  }
}

// Var 10:e minut: 24 h-historiken till flash.
static void saveHistory() {
  if (!s_fsOk) return;
  AppLock lock;
  histStoreSave(g_hist24);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  g_resetReason = resetReasonName(esp_reset_reason());
  Serial.printf("\nBMS v2 %s startar (%s)\n", FW_VERSION, g_resetReason);
  g_appMutex = xSemaphoreCreateMutex();
  settingsLoad(g_settings);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  balancerInit(s_balMem);
  alarmsInit(s_alarms);
  packInit(s_ps);
  memset(&g_snap, 0, sizeof(g_snap));
  packInit(g_snap.ps);
  histInit(g_hist24, s_buf24, HIST_24H_N);
  histInit(g_histLive, s_bufLive, HIST_LIVE_N);
  clInit(g_cellLog, s_bufCells, CELLLOG_N);
  cellAccumReset(g_cellAcc);
  accumReset(s_acc);
  currentSensorBegin();
  SocPersist sp;
  if (socLoad(sp)) {
    // Sparad laddnivå behövs bara för Ah-räkningen. Utan strömsensor räknas den direkt från
    // vilospänningen, annars skulle ett gammalt värde visas medan utjämningen hinner ikapp.
    socInit(s_soc, g_settings.curEnabled ? sp.pct : -1.0f);
    s_soc.ahIn = sp.ahIn;
    s_soc.ahOut = sp.ahOut;
    s_soc.kWhIn = sp.kWhIn;
    s_soc.kWhOut = sp.kWhOut;
    s_savedSocPct = sp.pct;
    s_savedAh = sp.ahIn + sp.ahOut;
  } else {
    socInit(s_soc, -1.0f);
  }
  if (!canBegin(g_settings.canPollMs)) Serial.println("MCP2515 svarar inte – kontrollera SPI-kopplingen");
  netBegin(g_settings);
  clockBegin();
  s_fsOk = histStoreBegin();
  if (s_fsOk) {
    histStoreLoad(g_hist24);
    cellLogStoreLoad(g_cellLog);
  }
  if (g_settings.espnowEnabled) s_espnowOn = espnowBegin();
  webBegin();
  Serial.printf("Eget WiFi: %s  ->  http://192.168.4.1  eller  http://bms.local\n", g_settings.apSsid);
}

void loop() {
  static uint32_t lastEval = 0, lastSec = 0, lastPkt = 0;
  const uint32_t now = millis();
  if (now - lastEval >= 100) {
    lastEval = now;
    evaluate();
  }
  if (now - lastSec >= 1000) {
    lastSec = now;
    everySecond();
  }
  static uint32_t lastMin = 0, last5 = 0, last10 = 0, lastHour = 0;
  if (now - lastMin >= 60000) {
    lastMin = now;
    everyMinute();
  }
  if (now - last5 >= 300000) {
    last5 = now;
    saveSocIfChanged();
  }
  if (now - last10 >= 600000) {
    last10 = now;
    saveHistory();
  }
  if (now - lastHour >= CELLLOG_PERIOD_MS) {
    lastHour = now;
    everyHour();
  }
  if (s_espnowOn && now - lastPkt >= 500) {
    lastPkt = now;
    sendPacket();
  }
  applySettingsIfDirty();
  checkBootButton(now);
  if (g_restartAtMs && (int32_t)(now - g_restartAtMs) >= 0) {
    saveSocIfChanged();
    saveHistory();
    ESP.restart();
  }
  delay(5);
}
