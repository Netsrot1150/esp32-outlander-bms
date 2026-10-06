#pragma once
// Delat läge mellan loop() och webben. Allt skyddas av g_appMutex (använd AppLock).
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "settings.h"
#include "cmu.h"
#include "balancer.h"
#include "alarms.h"
#include "can_bus.h"
#include "net.h"
#include "soc.h"
#include "current.h"
#include "history.h"
#include "celllog.h"

// Senaste beräknade läget. Skrivs av loop() var 100:e ms, läses av webben och ESP-NOW.
struct AppSnapshot {
  uint32_t        nowMs;
  PackState       ps;
  PackStats       st;
  BalanceDecision bal;
  AlarmState      al;
  CanCounters     can;
  NetStatus       net;
  uint32_t        bleedS[MAX_CMU_ID + 1][CELLS_PER_CMU];   // bleed-sekunder per cell sedan start/nollställning
  uint16_t        fps[MAX_CMU_ID + 1][CMU_SUBS];               // ramar per sekund per CMU och sub
  uint32_t        espnowSent;
  uint32_t        heapFree;
  uint32_t        heapMin;
  SocState        soc;
  CurrentStatus   cur;
  uint16_t        curMv;      // rå sensorspänning (0 om sensorn är avstängd)
  bool            timeKnown;
  uint32_t        epoch;
  bool            fsOk;
};

extern AppSnapshot       g_snap;
extern Settings          g_settings;        // gällande inställningar
extern SemaphoreHandle_t g_appMutex;
extern volatile bool     g_settingsDirty;   // webben har ändrat g_settings -> loop sparar i NVS
extern volatile bool     g_bleedResetReq;
extern volatile uint32_t g_restartAtMs;     // 0 = ingen omstart planerad
extern HistRing          g_hist24;          // 1 post/minut, sparas i flash
extern HistRing          g_histLive;        // 1 post/sekund, bara RAM
extern CellLogRing       g_cellLog;         // 1 post/timme med alla celler, sparas i flash
extern CellAccum         g_cellAcc;         // innevarande timme
extern const char*       g_resetReason;     // orsak till senaste omstart

struct AppLock {
  AppLock() { xSemaphoreTake(g_appMutex, portMAX_DELAY); }
  ~AppLock() { xSemaphoreGive(g_appMutex); }
  AppLock(const AppLock&) = delete;
  AppLock& operator=(const AppLock&) = delete;
};
