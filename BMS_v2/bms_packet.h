#pragma once
// ESP-NOW-paket från BMS:en till skärmenheten. Kopieras ordagrant till skärmprojektet.
// Little-endian (ESP32). Skickas som broadcast var 500:e ms på aktuell WiFi-kanal.
#include <stdint.h>

#define BMS_PACKET_VERSION 2
#define BMS_PACKET_MAX_CMU 8
#define BMS_TEMP_INVALID   INT8_MIN
#define BMS_NO_CURRENT     INT16_MIN

#pragma pack(push, 1)
struct BmsCmuPacket {
  uint8_t  id;
  uint8_t  flags;          // bit0 online, bit1 åtta celler
  uint8_t  balBits;        // bit k = cell k+1 bleeds
  uint16_t cellmV[8];
  int8_t   tempC[6];       // BMS_TEMP_INVALID = ingen givare
};

struct BmsPacket {
  uint8_t  magic[2];       // 'B', 'M'
  uint8_t  version;        // BMS_PACKET_VERSION
  uint8_t  seq;
  uint32_t uptimeS;
  uint32_t packmV;
  uint16_t minCellmV;
  uint16_t maxCellmV;
  uint8_t  minCellLoc;     // (modulindex << 4) | cellindex
  uint8_t  maxCellLoc;
  int8_t   minTempC;
  int8_t   maxTempC;
  uint8_t  balState;       // BalState: 0 av, 1 väntar, 2 aktiv, 3 manuell, 4 blockerad
  uint8_t  cmuCount;
  uint16_t balTargetmV;
  uint32_t alarmBits;      // bit = AlarmId
  uint32_t warnBits;
  BmsCmuPacket cmu[BMS_PACKET_MAX_CMU];
  uint16_t socX10;         // laddnivå 0,1 %
  int16_t  currentdA;      // ström 0,1 A, positiv = laddning, BMS_NO_CURRENT = ingen sensor
  uint8_t  socMode;        // 0 = vilospänning, 1 = Ah-räkning
};
#pragma pack(pop)

static_assert(sizeof(BmsCmuPacket) == 25, "BmsCmuPacket ska vara 25 byte");
static_assert(sizeof(BmsPacket) == 237, "BmsPacket ska vara 237 byte");
static_assert(sizeof(BmsPacket) <= 250, "ESP-NOW tillåter högst 250 byte");
