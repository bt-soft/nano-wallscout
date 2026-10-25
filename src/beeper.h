// Csipogo: Timer2 CTC, OC2A (D11) hardveres billentes, megszakitas nelkul
#pragma once

#include <Arduino.h>

void beepInit();
void beepOff();
void beepOn(uint16_t freqHz);
