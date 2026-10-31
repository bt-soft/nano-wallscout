#pragma once

#include <Arduino.h>

#include "config.h"

// Soros porton történő naplózás csak debug módban
#ifdef __DEBUG__

#include "app_types.h"

/**
 * @brief Kiírja a mérési eredményeket a soros portra.
 *
 * @param st A program globális állapota.
 * @param m Az aktuális mérési eredmény.
 * @param out Az üzemmód kimeneti adatai.
 */
void logSerial(const State &st, const Measurement &m, const Outcome &out) {

    // Üzemmód: S=DD-beállító, T=Jel-Ido, C=középkereső, R=normál.
    Serial.print(st.mode == OperatingMode::DD_SETUP ? 'S' : (st.mode == OperatingMode::SIGNAL_TIME ? 'T' : (st.mode == OperatingMode::CENTER_FINDER ? 'C' : 'R')));
    if (st.mode == OperatingMode::DD_SETUP) {
        Serial.print(F(" ch=A"));
        Serial.print(CHANNEL);
    }

    Serial.print(F(" diff=")); // Különbség a mérés és a nulla között
    Serial.print(m.diff, 2);

    Serial.print(F(" raw=")); // Nyers, nem simított különbség
    Serial.print(m.rawDiff, 2);

    Serial.print(F(" s1=")); // S1 mérés átlagértéke, a kikapcsolás után 60 µs-mal. Ekkor a tárgy örvényárama még jelen van.
    Serial.print(m.s1Mean, 0);

    Serial.print(F(" s2=")); // S2 mérés átlagértéke, a kikapcsolás után 600 µs-mal. Ekkor az örvényáram lecsengett, így ez az alapvonal (offset, drift).
    Serial.print(m.s2Mean, 0);

    Serial.print(F(" idle=")); // Inaktív állapot mérése (DC-hiba)
    Serial.print(m.idle);

    Serial.print(F(" zero=")); // Nulla (referencia) érték.
    Serial.print(st.zero, 2);

    Serial.print(F(" sig=")); // Jelszint (a detektor bemenete)
    Serial.print(out.sig, 2);

    Serial.print(F(" vbat=")); // Akkumulátor feszültség
    Serial.print(m.vbat, 2);

#if ENABLE_CENTER_FINDER_SCREEN
    if (isGraphMode(st.mode)) {
        Serial.print(F(" th="));
        Serial.print(st.runTh, 2);
        Serial.print(F(" pk="));
        Serial.print(st.centerFinder.peakMagnitude(), 2);
        Serial.print(F(" rp="));
        Serial.print(st.centerFinder.returnMagnitude(), 2);
    }
#endif // ENABLE_CENTER_FINDER_SCREEN

    Serial.println();
}
#endif // __DEBUG__