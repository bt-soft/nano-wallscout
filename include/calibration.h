#pragma once

/**
 * @brief Mélységkalibráció az S ≈ K / d³ modell és az EEPROM alapján.
 */

#include <Arduino.h>

/**
 * @brief Betölti és ellenőrzi az EEPROM-ban tárolt kalibrációs értéket.
 *
 */
void calLoad();

/**
 * @brief Megadja, hogy érvényes kalibráció van-e beállítva.
 * @return Igaz, ha a kalibrációs állandó érvényes.
 */
bool calIsSet();

/**
 * @brief Beállítja és az EEPROM-ba menti a kalibrációs állandót.
 * @param k A mentendő kalibrációs állandó.
 *
 */
void calSet(float k);

/**
 * @brief Becsült mélységet számít a mért jelből.
 * @param mag A jel abszolút értéke ADC-egységben.
 * @return A becsült mélység centiméterben.
 */
float calDepthCm(float mag);

/**
 * @brief Meghatározza a kijelzőskála teljes kitéréséhez tartozó jelszintet.
 * @return A teljes skálához tartozó jel ADC-egységben.
 */
float calFullScale();

/**
 * @brief Kiszámítja a kalibrációs állandót az ismert távolságban mért jelből.
 * @param mag A kalibrálási távolságban mért jel abszolút értéke.
 * @return A mélységmodell kalibrációs állandója.
 */
float calKFromSignal(float mag);
