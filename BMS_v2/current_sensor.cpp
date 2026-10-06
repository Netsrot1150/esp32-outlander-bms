#include "current_sensor.h"
#include <Arduino.h>
#include "config.h"

void currentSensorBegin() {
  pinMode(PIN_CUR_ADC, INPUT);
  analogSetPinAttenuation(PIN_CUR_ADC, ADC_11db);   // ca 0–3,1 V
}

uint16_t currentSensorReadMv(uint16_t zeromV, uint16_t sensX100) {
#ifdef SIMULATE_CMUS
  // Cykel på 20 min: laddning 10 A i 8 min, vila, urladdning 25 A i 8 min, vila.
  const uint32_t s = (millis() / 1000) % 1200;
  float a = 0.0f;
  if (s < 480) a = 10.0f;
  else if (s >= 600 && s < 1080) a = -25.0f;
  a += (float)((millis() / 100) % 7) * 0.05f - 0.15f;   // lite brus
  return (uint16_t)((float)zeromV + a * sensX100 / 100.0f);
#else
  (void)zeromV;
  (void)sensX100;
  uint32_t sum = 0;
  for (int i = 0; i < 64; i++) sum += analogReadMilliVolts(PIN_CUR_ADC);
  return (uint16_t)(sum / 64);
#endif
}
