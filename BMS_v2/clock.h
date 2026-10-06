#pragma once
// Klocka: NTP när hemnätet finns, annars tiden som webbläsaren skickar.
#include <stdint.h>

void clockBegin();
void clockLoop();                                // en gång per sekund
bool clockKnown();
uint32_t clockEpoch();                           // 0 om okänd
void clockSetEpoch(uint32_t epoch, bool fromNtp);
