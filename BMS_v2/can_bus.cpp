#include "can_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "balancer.h"
#include "mcp2515.h"
#include "sim.h"

namespace {

struct CapFrame {
  uint32_t us;
  uint16_t id;
  uint8_t  len;
  uint8_t  tx;
  uint8_t  data[8];
};

const size_t CAP_MAX = 2000;   // 2000 * 16 byte = 32 kB, behålls tills nästa inspelning
const char   CSV_HEADER[] = "Time Stamp,ID,Extended,Dir,Bus,LEN,D1,D2,D3,D4,D5,D6,D7,D8\n";
const size_t CSV_HEADER_LEN = sizeof(CSV_HEADER) - 1;
const size_t CSV_LINE_LEN = 57;   // fast radlängd, se formatLine()

Mcp2515           s_mcp;
SemaphoreHandle_t s_mutex = nullptr;
TaskHandle_t      s_task = nullptr;
PackState         s_ps;
CanCounters       s_cnt;
volatile uint16_t s_target = BAL_IDLE_TARGET_MV;
volatile uint8_t  s_mode = 0;
volatile uint16_t s_pollMs = 200;
uint8_t           s_txFail = 0;
bool              s_acceptAll = false;   // filtret avstängt i MCP2515 (under inspelning)

CapFrame*         s_cap = nullptr;
volatile size_t   s_capCount = 0;
volatile bool     s_capRunning = false;
uint32_t          s_capStartUs = 0;
uint32_t          s_capEndMs = 0;

void lock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(s_mutex); }

void IRAM_ATTR onCanInt() {
  BaseType_t woken = pdFALSE;
  if (s_task) vTaskNotifyGiveFromISR(s_task, &woken);
  if (woken) portYIELD_FROM_ISR();
}

// Anropas med mutex tagen.
void captureAppend(uint16_t id, uint8_t len, const uint8_t* d, bool tx) {
  if (!s_capRunning) return;
  if (s_capCount >= CAP_MAX || (int32_t)(millis() - s_capEndMs) >= 0) {
    s_capRunning = false;
    return;
  }
  CapFrame& f = s_cap[s_capCount];
  f.us = micros() - s_capStartUs;
  f.id = id;
  f.len = len > 8 ? 8 : len;
  f.tx = tx ? 1 : 0;
  memset(f.data, 0, sizeof(f.data));
  memcpy(f.data, d, f.len);
  s_capCount = s_capCount + 1;
}

void handleFrame(uint16_t id, uint8_t len, const uint8_t* d, bool tx) {
  lock();
  if (!tx) {
    if (decodeFrame(s_ps, id, d, len, millis())) s_cnt.rxFrames++;
    else s_cnt.rxOther++;
  }
  captureAppend(id, len, d, tx);
  unlock();
}

#ifdef SIMULATE_CMUS
void simEmit(uint16_t id, const uint8_t data[8], void*) { handleFrame(id, 8, data, false); }
#endif

void sendBalance() {
  const uint16_t t = s_target;
  const uint8_t m = s_mode;
  const uint8_t d[8] = {(uint8_t)(t >> 8), (uint8_t)(t & 0xFF), m, 0x04, 0x03, 0x00, 0x00, 0x00};
#ifdef SIMULATE_CMUS
  handleFrame(CAN_ID_BALANCE, 8, d, true);
  lock();
  s_cnt.txOk++;
  unlock();
  simStep(t, m, millis(), simEmit, nullptr);
#else
  const bool ok = s_mcp.send(CAN_ID_BALANCE, d, 8);
  bool aborted = false;
  if (ok) {
    s_txFail = 0;
  } else if (++s_txFail >= 5) {   // 5 missade sändningar i rad: rensa TX-bufferten
    s_mcp.abortTx();
    s_txFail = 0;
    aborted = true;
  }
  lock();
  if (ok) s_cnt.txOk++;
  else s_cnt.txBusy++;
  if (aborted) s_cnt.txAborts++;
  unlock();
  if (ok) handleFrame(CAN_ID_BALANCE, 8, d, true);
#endif
}

#ifndef SIMULATE_CMUS
// Initierar MCP2515 och sparar resultatet i räknarna. Anropas vid start och var 2:a s tills det lyckas.
bool initMcp() {
  const bool ok = s_mcp.begin(SPI, PIN_CAN_CS);
  lock();
  s_cnt.ok = ok;
  s_cnt.mcpStep = s_mcp.initStep();
  s_cnt.initCanstat = s_mcp.initCanstat();
  s_cnt.spiOk = s_mcp.spiOk();
  s_cnt.initTries++;
  unlock();
  return ok;
}

void pollErrors() {
  const uint8_t tec = s_mcp.tec(), rec = s_mcp.rec(), ef = s_mcp.eflg();
  const uint8_t cs = s_mcp.canstat(), cc = s_mcp.canctrl();
  const uint8_t ov = s_mcp.takeOverflows();
  lock();
  s_cnt.tec = tec;
  s_cnt.rec = rec;
  s_cnt.eflg = ef;
  s_cnt.canstat = cs;
  s_cnt.canctrl = cc;
  s_cnt.overflows += ov;
  if (ef & 0x38) s_cnt.lastErrorMs = millis() | 1;   // error passive eller bus-off (overflow räknas separat)
  unlock();

  // Vakt: har kretsen lämnat normalläget (t.ex. startat om av en spänningsdipp och står i
  // konfigurationsläge) i 3 avläsningar i rad initieras den om via samma väg som vid start.
  static uint8_t notNormal = 0;
  notNormal = (cs & 0xE0) == 0x00 ? 0 : (uint8_t)(notNormal + 1);
  if (notNormal >= 3) {
    notNormal = 0;
    lock();
    s_cnt.ok = false;   // canTask försöker initiera igen
    s_cnt.reinits++;
    s_cnt.lastErrorMs = millis() | 1;
    unlock();
  }
}
#endif

void canTask(void*) {
  uint32_t lastTx = 0, lastErr = 0, lastInit = 0;
  for (;;) {
#ifdef SIMULATE_CMUS
    vTaskDelay(pdMS_TO_TICKS(5));
#else
    if (!s_cnt.ok) {   // MCP2515 svarade inte: försök igen var 2:a sekund
      vTaskDelay(pdMS_TO_TICKS(50));
      if (millis() - lastInit >= 2000) {
        lastInit = millis();
        initMcp();
      }
      continue;
    }
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5));   // väcks av INT, annars senast efter 5 ms
    CanFrame f;
    for (uint8_t n = 0; n < 32 && s_mcp.read(f); n++) {
      if (!f.ext && !f.rtr) {
        handleFrame((uint16_t)f.id, f.len, f.data, false);
      } else {
        lock();
        s_cnt.rxOther++;
        unlock();
      }
    }
#endif
    const uint32_t now = millis();
    if (now - lastTx >= s_pollMs) {
      lastTx = now;
      sendBalance();
    }
#ifndef SIMULATE_CMUS
    if (now - lastErr >= 100) {
      lastErr = now;
      pollErrors();
    }
    if (s_capRunning != s_acceptAll) {   // inspelningen ska se alla ramar, inte bara CMU-ramarna
      s_acceptAll = s_capRunning;
      s_mcp.setAcceptAll(s_acceptAll);
    }
#endif
    if (s_capRunning && (int32_t)(now - s_capEndMs) >= 0) {
      lock();
      s_capRunning = false;
      unlock();
    }
  }
}

size_t formatLine(char* out, const CapFrame& f) {
  return (size_t)snprintf(out, CSV_LINE_LEN + 1,
                          "%010lu,%08lX,false,%s,0,%u,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
                          (unsigned long)f.us, (unsigned long)f.id, f.tx ? "Tx" : "Rx", (unsigned)f.len,
                          f.data[0], f.data[1], f.data[2], f.data[3], f.data[4], f.data[5], f.data[6], f.data[7]);
}

}  // namespace

bool canBegin(uint16_t pollMs) {
  s_mutex = xSemaphoreCreateMutex();
  packInit(s_ps);
  memset(&s_cnt, 0, sizeof(s_cnt));
  s_pollMs = pollMs;
  s_cnt.pollMs = pollMs;
#ifdef SIMULATE_CMUS
  s_cnt.ok = true;
  s_cnt.simulated = true;
  simReset();
#else
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);
  initMcp();
#endif
  xTaskCreate(canTask, "can", 4096, nullptr, 15, &s_task);   // startas alltid; försöker igen om MCP2515 saknas
#ifndef SIMULATE_CMUS
  pinMode(PIN_CAN_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_CAN_INT), onCanInt, FALLING);
#endif
  return s_cnt.ok;
}

void canSetBalance(uint16_t targetmV, uint8_t mode) {
  s_target = targetmV;
  s_mode = mode;
}

void canSetPollMs(uint16_t ms) {
  s_pollMs = ms;
  if (!s_mutex) return;
  lock();
  s_cnt.pollMs = ms;
  unlock();
}

void canSnapshot(PackState& ps, CanCounters& cnt) {
  if (!s_mutex) {
    packInit(ps);
    memset(&cnt, 0, sizeof(cnt));
    return;
  }
  lock();
  ps = s_ps;
  cnt = s_cnt;
  unlock();
}

bool canCaptureStart(uint16_t seconds) {
  if (!s_mutex) return false;
  lock();
  if (!s_cap) s_cap = (CapFrame*)malloc(CAP_MAX * sizeof(CapFrame));
  const bool ok = s_cap != nullptr;
  if (ok) {
    s_capCount = 0;
    s_capStartUs = micros();
    s_capEndMs = millis() + (uint32_t)seconds * 1000UL;
    s_capRunning = true;
  }
  unlock();
  return ok;
}

bool canCaptureRunning() { return s_capRunning; }
size_t canCaptureCount() { return s_capCount; }
size_t canCaptureCsvSize() { return CSV_HEADER_LEN + s_capCount * CSV_LINE_LEN; }

size_t canCaptureCsvRead(uint8_t* buf, size_t maxLen, size_t pos) {
  if (s_capRunning || !s_cap) return 0;
  const size_t total = canCaptureCsvSize();
  size_t written = 0;
  char line[CSV_LINE_LEN + 1];
  while (written < maxLen && pos < total) {
    const char* src;
    size_t srcLen, off;
    if (pos < CSV_HEADER_LEN) {
      src = CSV_HEADER;
      srcLen = CSV_HEADER_LEN;
      off = pos;
    } else {
      const size_t i = (pos - CSV_HEADER_LEN) / CSV_LINE_LEN;
      off = (pos - CSV_HEADER_LEN) % CSV_LINE_LEN;
      formatLine(line, s_cap[i]);
      src = line;
      srcLen = CSV_LINE_LEN;
    }
    size_t n = srcLen - off;
    if (n > maxLen - written) n = maxLen - written;
    memcpy(buf + written, src + off, n);
    written += n;
    pos += n;
  }
  return written;
}
