// Melysegkalibracio: S ~ K / d^3, K az EEPROM-ban
#pragma once

#include <Arduino.h>

void calLoad();
bool calIsSet();
void calSet(float k);  // beallitja es EEPROM-ba menti

// Mert jelbol (ADC egyseg) becsult melyseg cm-ben; csak kalibralt allapotban ertelmes
float calDepthCm(float mag);

// Az oszlop teljes kiteresehez tartozo jelszint
float calFullScale();

// K ertek egy jelbol, amelyet a kalibralasi melysegben mertunk
float calKFromSignal(float mag);
