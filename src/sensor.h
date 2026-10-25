// Impulzus-meghajtas, ADC mintavetelezes, tap- es DC-ellenorzes, gombfigyeles
#pragma once

#include <Arduino.h>

// A gombok allapota a legutobbi measure() alatt (mintavetelezve, hogy a rovid nyomasok ne vesszenek el)
extern bool g_zeroSeen, g_calSeen;

void sensorInit();

// N_PULSES impulzus osszegzett S1 / S2 mintai az adott ADC csatornan (ch == 2: DD csatolas-meres)
void measure(uint32_t& sum1, uint32_t& sum2, uint8_t ch);

// A0 szintje impulzus nelkul, a telitesbol visszaterve (8 minta atlaga)
uint16_t readIdleLevel();

// Akkufeszultseg (V): Vcc a belso 1,1 V-os referenciabol, az akku az A1-en
float readBattery();
