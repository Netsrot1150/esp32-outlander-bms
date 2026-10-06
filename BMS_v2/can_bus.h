#pragma once
// CAN-task: läser MCP2515 (avbrottsstyrt), avkodar CMU-ramar, skickar 0x3C3 och spelar in ramar.
#include <Arduino.h>
#include "cmu.h"

struct CanCounters {
  bool     ok;            // MCP2515 initierad (alltid true i simulering)
  bool     simulated;
  uint32_t rxFrames;      // CMU-ramar
  uint32_t rxOther;       // övriga ramar
  uint32_t txOk;
  uint32_t txBusy;        // TXB0 upptagen när 0x3C3 skulle skickas
  uint32_t txAborts;
  uint32_t overflows;     // RX-overflow i MCP2515
  uint8_t  tec, rec, eflg;
  uint8_t  mcpStep;       // McpInitStep från senaste initieringsförsöket
  uint8_t  initCanstat;   // CANSTAT direkt efter reset vid senaste försöket
  uint8_t  canstat;       // aktuellt CANSTAT (bit 7-5 = läge: 000 normal, 100 konfiguration)
  uint8_t  canctrl;
  bool     spiOk;         // registertest vid senaste initieringen
  uint32_t initTries;
  uint32_t reinits;       // omstarter av MCP2515 efter att den lämnat normalläget
  uint32_t lastErrorMs;   // senaste error passive/bus-off, 0 = aldrig
  uint16_t pollMs;
};

bool canBegin(uint16_t pollMs);                    // false om MCP2515 inte svarar (nytt försök var 2:a s)
void canSetBalance(uint16_t targetmV, uint8_t mode);
void canSetPollMs(uint16_t ms);
void canSnapshot(PackState& ps, CanCounters& cnt); // kopia under mutex

// CAN-inspelning till RAM, exporteras som SavvyCAN-CSV med fast radlängd.
bool   canCaptureStart(uint16_t seconds);          // false om minne saknas
bool   canCaptureRunning();
size_t canCaptureCount();
size_t canCaptureCsvSize();
size_t canCaptureCsvRead(uint8_t* buf, size_t maxLen, size_t pos);   // 0 = slut
