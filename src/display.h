// SSD1306 OLED kijelzo
#pragma once

#include <Arduino.h>

void displayInit();

// Normal (kereso) mod: szam, becsult melyseg, oszlop; msg != nullptr eseten az uzenet
void drawNormal(const char* msg, float mag, float vbat, bool overload, bool det);

// DD beallito mod: elojeles S1 - S2, kulon sorban a minimum es a nyers S1 (telites-ellenorzes)
void drawSetup(float diff, float best, float s1, bool overload, bool dcFault, float vbat);
