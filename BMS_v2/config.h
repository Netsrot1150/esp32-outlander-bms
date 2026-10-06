#pragma once
// BMS v2 – gemensamma konstanter. Ingen Arduino-kod här (används även av PC-testerna).

#define FW_VERSION "2.3.2"

// Avkommentera för att köra utan batteri: CAN-tasken genererar 8 simulerade CMU:er.
// (tools/build.py --sim sätter den via kompilatorflagga.)
// #define SIMULATE_CMUS

// ---- Pinnar (ESP32-C3) ----
#define PIN_SPI_SCK   4
#define PIN_SPI_MISO  5
#define PIN_SPI_MOSI  6
#define PIN_CAN_CS    7
#define PIN_CAN_INT   0
#define PIN_BOOT_BTN  9
#define PIN_CUR_ADC   3     // WCS1700 AO (ADC1 kanal 3)

// ---- CMU-layout ----
#define MAX_CMU_ID     15    // CMU-id 1..15 (bitarna 4-7 i 0x6X1)
#define CELLS_PER_CMU  8
#define TEMPS_PER_CMU  6     // byte 2..7 i 0x6X1, 0xFF = ingen givare
#define CMU_SUBS       5     // ramtyper 0x6X1..0x6X5
#define CAN_ID_BALANCE 0x3C3

// ---- Giltig cellspänning ----
#define CELL_VALID_MIN_MV 2000
#define CELL_VALID_MAX_MV 4500

// ---- Utjämning av cellspänningar ----
// Exponentiellt medel med tidskonstanten CELL_EMA_N ramar (ca 4 s vid CMU:ernas 25 Hz).
#define CELL_EMA_N     100
#define STICKY_LOC_Q   (1u << 15)   // 0,5 mV i cellQ-enheter: hysteres för min/max-platsen

// ---- WiFi-standardvärden. Lösenorden ligger i secrets.h som inte versionshanteras. ----
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
