#include "current.h"

CurrentStatus currentFromMv(uint16_t mv, const Settings& s) {
  CurrentStatus c;
  c.enabled = s.curEnabled;
  c.amps = 0.0f;
  c.saturated = false;
  if (!s.curEnabled || s.curSensX100 == 0) return c;
  const float factor = s.curFactorX10 / 10.0f;
  const float limit = CUR_SENSOR_MAX_A * factor;
  float a = ((float)mv - (float)s.curZeromV) / (s.curSensX100 / 100.0f) * factor;
  if (s.curInvert) a = -a;
  if (mv < CUR_ADC_MIN_MV || mv > CUR_ADC_MAX_MV) c.saturated = true;
  if (a > limit) { a = limit; c.saturated = true; }
  if (a < -limit) { a = -limit; c.saturated = true; }
  c.amps = a;
  return c;
}
