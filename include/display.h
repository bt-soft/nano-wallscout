/**
 * @brief Az SSD1306 OLED-kijelző megjelenítési felülete.
 */
#pragma once

#include <Arduino.h>

#include "center_finder.h"

/**
 * @brief Inicializálja az I²C-kapcsolatot és az OLED-kijelzőt.
 *
 */
void displayInit();

/**
 * @brief Megjeleníti a kereső normál üzemmódjának adatait.
 * @param msg Opcionális állapotüzenet; null érték esetén a mérési adat jelenik meg.
 * @param mag A jel abszolút értéke.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param det Igaz, ha a jel alapján találat történt.
 *
 */
void drawNormal(const char *msg, float mag, float vbat, bool overload, bool det);

/**
 * @brief Megjeleníti a távolságtanító képernyőt: az élő jelet és a becsült távolságot.
 * @param msg Opcionális állapotüzenet.
 * @param mag A nullázott jel abszolút értéke.
 * @param threshold A detektálási küszöb; ez alatt nem jelenik meg távolság.
 * @param vbat Az akkumulátor feszültsége voltban.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 *
 */
void drawTeach(const char *msg, float mag, float threshold, float vbat, bool overload);

/**
 * @brief Megjeleníti a középkeresés állapotát és az előjeles jel rövid történetét.
 */
void drawCenter(const char *msg, float sig, float threshold, float peak, float returnPeak, float vbat,
                bool overload, bool dcFault, bool historyMode, CenterStage stage,
                const int8_t *history, uint8_t historyCount, uint8_t historyNext);

/**
 * @brief Megjeleníti az áthallást és a telítési adatokat DD-beállító módban.
 * @param diff Az előjeles S1 − S2 különbség.
 * @param best Az eddig mért legkisebb abszolút áthallás.
 * @param s1 Az S1-minta nyers ADC-értéke.
 * @param overload Igaz, ha az erősítő túlvezérlésben van.
 * @param dcFault Igaz, ha az A0 nyugalmi DC-szintje hibás.
 * @param vbat Az akkumulátor feszültsége voltban.
 *
 */
void drawSetup(float diff, float best, float s1, bool overload, bool dcFault, float vbat);
