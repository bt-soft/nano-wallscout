/**
 * @brief Impulzusvezérlés, ADC-mintavételezés, táp- és DC-ellenőrzés, gombfigyelés.
 */
#pragma once

#include <Arduino.h>

/**
 * @brief A legutóbbi mérés közben észlelt gombnyomások.
 *
 * A mintavételezés megakadályozza, hogy a rövid gombnyomások elveszjenek.
 */
extern bool g_zeroSeen, g_calSeen;

/**
 * @brief Inicializálja az érzékelőt, az ADC-t, az időzítőt és a bemeneteket.
 * @return Nincs visszatérési érték.
 */
void sensorInit();

/**
 * @brief Összegzi az S1 és S2 mintákat a megadott ADC-csatornán.
 * @param sum1 Az S1-minták összegének kimeneti hivatkozása.
 * @param sum2 Az S2-minták összegének kimeneti hivatkozása.
 * @param ch Az ADC-csatorna; a 2-es csatorna a DD-csatolás mérésére szolgál.
 * @return Nincs visszatérési érték.
 */
void measure(uint32_t& sum1, uint32_t& sum2, uint8_t ch);

/**
 * @brief Megméri az A0 nyugalmi szintjét nyolc minta átlagából.
 * @return Az A0 bemeneten mért átlagos ADC-érték.
 */
uint16_t readIdleLevel();

/**
 * @brief Megméri az akkumulátor feszültségét a belső referencia és az A1 alapján.
 * @return Az akkumulátor becsült feszültsége voltban.
 */
float readBattery();
