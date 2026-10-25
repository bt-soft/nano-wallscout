/**
 * @brief Az SSD1306 OLED-kijelző megjelenítési felülete.
 */
#pragma once

#include <Arduino.h>

/**
 * @brief Inicializálja az I²C-kapcsolatot és az OLED-kijelzőt.
 * @return Nincs visszatérési érték.
 */
void displayInit();

/**
 * @brief Megjeleníti a kereső normál üzemmódjának adatait.
 * @param msg Opcionális állapotüzenet; null érték esetén a mérési adat jelenik meg.
 * @param mag A jel abszolút értéke.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param det Igaz, ha a jel alapján találat történt.
 * @return Nincs visszatérési érték.
 */
void drawNormal(const char* msg, float mag, float vbat, bool overload, bool det);

/**
 * @brief Megjeleníti az áthallást és a telítési adatokat DD-beállító módban.
 * @param diff Az előjeles S1 − S2 különbség.
 * @param best Az eddig mért legkisebb abszolút áthallás.
 * @param s1 Az S1-minta nyers ADC-értéke.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha az A0 nyugalmi DC-szintje hibás.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @return Nincs visszatérési érték.
 */
void drawSetup(float diff, float best, float s1, bool overload, bool dcFault, float vbat);
