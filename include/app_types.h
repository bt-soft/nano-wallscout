#pragma once

#include <stdint.h>

#include "config.h"
#include "operating_mode.h"
#include "signal_graph.h"
#if ENABLE_CENTER_FINDER_SCREEN
#include "center_finder.h"
#endif

/**
 * @brief Egy mérési ciklus összesített eredménye.
 */
struct Measurement {
    /** Az S1- és S2-minták átlagai ADC-egységben. */
    float s1Mean, s2Mean;

    /** Az S1 − S2 nyers, nem simított különbsége. */
    float rawDiff;

    /** Az S1 − S2 különbség simított értéke. */
    float diff;

    /** Az A0 bemenet impulzus nélküli szintje. */
    uint16_t idle;

    /** Igaz, ha az erősítő túlvezérlésben van. */
    bool overload;

    /** Igaz, ha a bemenet nyugalmi DC-szintje hibás. */
    bool dcFault;

    /** Az akkumulátor mért feszültsége voltban. */
    float vbat;
};

/**
 * @brief Az üzemmód-logika által előállított hang- és kijelzési adatok.
 */
struct Outcome {
    /** A hangjelző frekvenciája hertzben; nulla esetén néma. */
    uint16_t freq = 0;

    /** A nullázott jel abszolút értéke. */
    float mag = 0;

    /** A nullázott, előjeles jel. */
    float sig = 0;

    /** Igaz, ha találat történt. */
    bool det = false;

    /** Opcionális, a kijelzőn megjelenő állapotüzenet. */
    const char *msg = nullptr;
};

/**
 * @brief A főciklus állapotát, a gombokat és a simítás adatait tárolja.
 */
struct State {
    /** A készülék aktuális mérési és kijelzési üzemmódja. */
    OperatingMode mode = OperatingMode::SEARCH;

    /** Igaz, ha a tanítóképernyőn rögzítést kértek. */
    bool teachRequest = false;

    /** A sikertelen tanítás üzenetének lejárati ideje. */
    uint32_t teachFailUntil = 0;

    /** A Jel-Ido és a középkereső nézet gördülő amplitúdógrafikonja. */
    SignalGraph graph;
#if ENABLE_CENTER_FINDER_SCREEN
    CenterFinder centerFinder;
#endif

    /** A kalibráló- és nullázógomb lenyomásának kezdőideje. */
    uint32_t calPressStart = 0, zeroPressStart = 0;

    /** A kalibrálás befejezett és a hosszú gombnyomás észlelt állapota. */
    bool calDone = false, zeroLong = false;

    /** A hangjelzés és a kalibrálási visszajelzés lejárati ideje. */
    uint32_t confirmUntil = 0, flashUntil = 0;

    /** A nullázáshoz összegyűjtött minták száma és összesítése. */
    uint16_t zeroCount = 0;
    float zeroAcc = 0, zeroSq = 0, zero = 0;

    /** A zajszinthez igazított futási küszöb. */
    float runTh = DETECT_TH;

    /** A hiszterézises találatjelzés aktuális állapota. */
    bool detOn = false;

    /** A DD-beállító módban eddig mért legkisebb abszolút áthallás. */
    float best = 1e9f;

    /** Az exponenciálisan simított mérési érték és annak csatornája. */
    float smooth = 0;
    uint8_t smoothCh = 255;
};
