#include <EEPROM.h>
#include <float.h>
#include <math.h>

#include "calibration.h"
#include "config.h"

static float g_calK = 0; // A kalibrációs állandó tárolása.

// A kalibrációs mélység köbének előre kiszámított értéke.
static const float CAL_DEPTH_CUBED = CAL_DEPTH_CM * CAL_DEPTH_CM * CAL_DEPTH_CM;

/**
 * @brief Betölti az EEPROM-ból és tartomány szerint ellenőrzi a kalibrációt.
 */
void calLoad() {
    EEPROM.get(0, g_calK); // Betölti a kalibrációs állandót az EEPROM-ból.

    // Ellenőrzi, hogy a kalibrációs állandó érvényes tartományban van-e.
    if (!(g_calK > 0 && g_calK < FLT_MAX)) {
        g_calK = 0; // Érvénytelen érték esetén alaphelyzetbe állítja a kalibrációt.
    }
}

/**
 * @brief Ellenőrzi, hogy rendelkezésre áll-e kalibrációs állandó.
 *
 * @return Igaz, ha a kalibrációs állandó pozitív.
 */
bool calIsSet() { return g_calK > 0; }

/**
 * @brief Eltárolja a kalibrációs állandót a memóriában és az EEPROM-ban.
 *
 * @param k A tárolandó kalibrációs állandó.
 *
 */
void calSet(float k) {
    g_calK = k;            // Beállítja a kalibrációs állandót.
    EEPROM.put(0, g_calK); // Elmenti az EEPROM-ba.
}

/**
 * @brief A jelből kiszámítja a becsült mélységet.
 *
 * @param mag A mért jel abszolút értéke.
 * @return A becsült mélység centiméterben.
 */
float calDepthCm(float mag) { return cbrt(g_calK / mag); }

/**
 * @brief Kiszámítja a kijelző teljes kitéréséhez tartozó jelszintet.
 *
 * @return A skála teljes kitéréséhez tartozó ADC-jel.
 */
float calFullScale() { return g_calK > 0 ? g_calK / CAL_DEPTH_CUBED * 1.5f : FULL_SCALE; }

/**
 * @brief Meghatározza a mélységmodellt leíró állandót kalibráláskor.
 *
 * @param mag Az ismert mélységben mért jel abszolút értéke.
 * @return A mélységmodell kalibrációs állandója.
 */
float calKFromSignal(float mag) { return mag * CAL_DEPTH_CUBED; }
