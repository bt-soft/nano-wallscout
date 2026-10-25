#include "calibration.h"

#include <EEPROM.h>
#include <math.h>

#include "config.h"

static float g_calK = 0;

static const float CAL_DEPTH_CUBED = CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM;

/**
 * @brief Betölti az EEPROM-ból és tartomány szerint ellenőrzi a kalibrációt.
 * @return Nincs visszatérési érték.
 */
void calLoad() {
  EEPROM.get(0, g_calK);
  if (!(g_calK > 0 && g_calK < 1e9f)) g_calK = 0;
}

/**
 * @brief Ellenőrzi, hogy rendelkezésre áll-e kalibrációs állandó.
 * @return Igaz, ha a kalibrációs állandó pozitív.
 */
bool calIsSet() { return g_calK > 0; }

/**
 * @brief Eltárolja a kalibrációs állandót a memóriában és az EEPROM-ban.
 * @param k A tárolandó kalibrációs állandó.
 * @return Nincs visszatérési érték.
 */
void calSet(float k) {
  g_calK = k;
  EEPROM.put(0, g_calK);
}

/**
 * @brief A jelből kiszámítja a becsült mélységet.
 * @param mag A mért jel abszolút értéke.
 * @return A becsült mélység centiméterben.
 */
float calDepthCm(float mag) { return cbrt(g_calK / mag); }

/**
 * @brief Kiszámítja a kijelző teljes kitéréséhez tartozó jelszintet.
 * @return A skála teljes kitéréséhez tartozó ADC-jel.
 */
float calFullScale() { return g_calK > 0 ? g_calK / CAL_DEPTH_CUBED * 1.5f : FULL_SCALE; }

/**
 * @brief Meghatározza a mélységmodellt leíró állandót kalibráláskor.
 * @param mag Az ismert mélységben mért jel abszolút értéke.
 * @return A mélységmodell kalibrációs állandója.
 */
float calKFromSignal(float mag) { return mag * CAL_DEPTH_CUBED; }
