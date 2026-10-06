#include "clock.h"
#include <Arduino.h>
#include <time.h>

static const uint32_t VALID_AFTER = 1700000000UL;   // 2023-11
static bool s_known = false;
static bool s_ntp = false;
static uint32_t s_offset = 0;   // epoch - millis()/1000

void clockBegin() { configTime(0, 0, "pool.ntp.org", "time.google.com"); }

void clockSetEpoch(uint32_t epoch, bool fromNtp) {
  if (epoch < VALID_AFTER) return;
  if (s_ntp && !fromNtp) return;   // NTP går före webbläsarens tid
  s_offset = epoch - millis() / 1000;
  s_known = true;
  if (fromNtp) s_ntp = true;
}

void clockLoop() {
  const time_t now = time(nullptr);
  if ((uint32_t)now > VALID_AFTER) clockSetEpoch((uint32_t)now, true);
}

bool clockKnown() { return s_known; }
uint32_t clockEpoch() { return s_known ? s_offset + millis() / 1000 : 0; }
