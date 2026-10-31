/**
 * @brief Az SSD1306 OLED-kijelző megjelenítési felülete.
 */
#pragma once

#include <Arduino.h>

#include "config.h"
#include "signal_graph.h"
#if ENABLE_CENTER_FINDER_SCREEN
#include "center_finder.h"
#endif

/**
 * @brief Inicializálja az I²C-kapcsolatot és az OLED-kijelzőt.
 *
 */
void displayInit();

/**
 * @brief Kikapcsolja az OLED-et (power save), és leállítja az I²C-buszt.
 * Ébresztéskor a displayInit() újra inicializálja.
 */
void displaySleep();

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
 * @param orient A vas helyzete: +1 párhuzamos ('|'), -1 merőleges ('-'), 0 ismeretlen.
 *
 */
void drawTeach(const char *msg, float mag, float threshold, float vbat, bool overload, int8_t orient);

/**
 * @brief Megjeleníti a Jel-Ido képernyőt: az előjeles jelet, a vas helyzetét és a gördülő grafikont.
 */
void drawSignalTime(float sig, float threshold, float vbat, bool overload, bool dcFault, const SignalGraph &graph);

#if ENABLE_CENTER_FINDER_SCREEN
/**
 * @brief Megjeleníti a középkereső állapotát, a csúcs- és visszatérési értéket és a grafikont.
 */
void drawCenterFinder(const char *msg, float sig, float threshold, float peak, float returnPeak, float vbat, bool overload, bool dcFault, CenterStage stage, const SignalGraph &graph);
#endif

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
void drawDDSetup(float diff, float best, float s1, bool overload, bool dcFault, float vbat);

/**
 * @brief A DD-beállító áthallásának logaritmikus aránya a teljes skálához képest.
 * @param a Az áthallás abszolút értéke.
 * @return Az arány 0 és 1 között.
 */
float setupFraction(float a);
