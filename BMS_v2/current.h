#pragma once
// Omräkning av WCS1700-hallsensorns utsignal till ström. Ren kod (ingen Arduino).
// Teckenkonvention: positiv ström = laddning (in i batteriet), negativ = urladdning.
#include <stdint.h>
#include "settings.h"

static const uint16_t CUR_ADC_MIN_MV = 50;       // under/över detta räknas sensorn som mättad
static const uint16_t CUR_ADC_MAX_MV = 3100;
static const float    CUR_SENSOR_MAX_A = 70.0f;  // WCS1700 mäter ±70 A

struct CurrentStatus {
  bool  enabled;     // curEnabled
  float amps;
  bool  saturated;
};

CurrentStatus currentFromMv(uint16_t mv, const Settings& s);
