/**
 * @file Arduino Nano-alapú, DD-tekercses betonvas-kereső vezérlőprogramja.
 *
 * Az impulzusmérés egyetlen szálon fut; a kritikus mintavételezés alatt
 * a megszakítások tiltva vannak.
 * A Timer1 időalapja 0,5 µs, a hangjelzőt a D3-as Timer2-kimenet (OC2B) vezérli.
 *
 */

// A Soros naplózás és az AVR8-stub hibakeresés együtt nem használható!!
// https://docs.platformio.org/en/stable/plus/debug-tools/avr-stub.html
// #define __DEBUG__
// #define __DEBUG_SERIAL_LOGGING__

#include <Arduino.h>
#if defined(__DEBUG__) && !defined(__DEBUG_SERIAL_LOGGING__)
#include <app_api.h>
#include <avr8-stub.h>
#endif

#include <avr/wdt.h>
#include <math.h>

#include "beeper.h"
#include "calibration.h"
#include "center_finder.h"
#include "config.h"
#include "display.h"
#include "sensor.h"
#include "utils.h"

/**
 * @brief A mérőjel ADC-csatornája; a beállító mód is ezt a jelutat használja.
 */
static const uint8_t CHANNEL = 0;

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
    /** Igaz, ha a DD-beállító mód aktív. */
    bool setupMode = false;

    /** Igaz, ha a külön középkereső mód aktív. */
    bool centerMode = false;

    /** Igaz, ha a távolságtanító képernyő aktív; kérés esetén az aktuális jel lesz az 5 cm-es érték. */
    bool teachMode = false, teachRequest = false;
    uint32_t teachFailUntil = 0;
    bool centerHistoryMode = false;

    /** Fix méretű gördülő amplitúdógrafikon a középkereső módhoz. */
    int8_t centerHistory[GRAPH_HISTORY_SIZE] = {};
    uint8_t centerHistoryCount = 0, centerHistoryNext = 0;
    CenterFinder centerFinder;

    /** A kalibráló- és nullázógomb lenyomásának kezdőideje. */
    uint32_t calPressStart = 0, zeroPressStart = 0;

    /** A kalibrálás befejezett és a hosszú gombnyomás észlelt állapota. */
    bool calDone = false, zeroLong = false;

    /** A hangjelzés és a kalibrálási visszajelzés lejárati ideje. */
    uint32_t confirmUntil = 0, flashUntil = 0;

    /** A nullázáshoz összegyűjtött minták száma és összesítése. */
    uint16_t zeroCount = 0;
    float zeroAcc = 0, zeroSq = 0, zero = 0;
    float zeroS1Acc = 0, zeroS2Acc = 0, zeroRawDiffAcc = 0;
    float baselineS1 = 0, baselineS2 = 0, baselineDiff = 0;

    /** A zajszinthez igazított futási küszöb. */
    float runTh = DETECT_TH;

    /** A hiszterézises találatjelzés aktuális állapota. */
    bool detOn = false;

    /** Az aktuális találat előjele; ellenkező előjelnél a jel el van nyomva. */
    int8_t signalPolarity = 0;

    /** Találat utáni visszaállás; stabil alapvonal-visszatérésig a nulla befagy. */
    bool recoveryActive = false;
    uint8_t recoveryCount = 0;
    float recoveryS1Acc = 0, recoveryS2Acc = 0, recoveryDiffAcc = 0;

    /** A DD-beállító módban eddig mért legkisebb abszolút áthallás. */
    float best = 1e9f;

    /** Az exponenciálisan simított mérési érték és annak csatornája. */
    float smooth = 0;
    uint8_t smoothCh = 255;
};

/** @brief A program globális mérési és üzemmód-állapota. */
static State st;

/**
 * @brief Simítja az S1 − S2 különbséget exponenciális átlagolással.
 * @param diff Az új, simítandó mérési különbség.
 * @return A simított különbség.
 */
static float smoothDiff(float diff) {

    // Ha a csatorna megváltozott, vagy a különbség hirtelen ugrik, állítsuk vissza a simított értéket.
    if (CHANNEL != st.smoothCh || fabsf(diff - st.smooth) > SMOOTH_JUMP) {
        st.smooth = diff;
        st.smoothCh = CHANNEL;
    } else {
        st.smooth += (st.setupMode ? SMOOTH_SETUP : SMOOTH_RUN) * (diff - st.smooth);
    }

    return st.smooth;
}

/**
 * @brief Elvégzi az impulzusmérést, és kiszámítja a mérés állapotjelzőit.
 * @return Az átlagolt mintákat, a nyugalmi szintet és az akkufeszültséget tartalmazó mérés.
 */
static Measurement acquire() {
    Measurement m;
    uint32_t s1, s2;

    measure(s1, s2, CHANNEL); // impulzusmérés a megadott csatornán

    wdt_reset(); // watchdog reset a rendszer biztonsága érdekében
    delay(3);

    m.idle = readIdleLevel(); // nyugalmi szint olvasása
    m.vbat = readBattery();   // akkufeszültség olvasása

    // Az S1 és S2 átlagok kiszámítása.
    m.s1Mean = (float)s1 / N_PULSES;
    m.s2Mean = (float)s2 / N_PULSES;

    m.rawDiff = (float)((int32_t)s1 - (int32_t)s2) / N_PULSES;

    // A simított különbség kiszámítása az S1 és S2 átlagok alapján.
    m.diff = smoothDiff(m.rawDiff);

    // A túlterhelés és DC-hiba jelzőinek beállítása.
    m.overload = m.s1Mean > OVERLOAD_HIGH || m.s1Mean < OVERLOAD_LOW;

    // A DC-hiba jelző beállítása.
    m.dcFault = m.idle > IDLE_HIGH || m.idle < IDLE_LOW;

    return m;
}

/**
 * @brief Újrakezdi az indulási vagy kézi nullázás mintagyűjtését.
 *
 */
static void restartZeroing() {
    st.zeroCount = 0;
    st.zeroAcc = 0;
    st.signalPolarity = 0;
    st.recoveryActive = false;
    st.recoveryCount = 0;
    st.recoveryS1Acc = 0;
    st.recoveryS2Acc = 0;
    st.recoveryDiffAcc = 0;
    st.detOn = false;
    st.centerFinder.reset();
    st.centerHistoryCount = 0;
    st.centerHistoryNext = 0;
    st.teachMode = false;
    st.teachRequest = false;

    // A simított különbség és a csatorna inicializálása.
    st.smooth = 0;
    st.smoothCh = 255;
}

/**
 * @brief Kezeli a nullázógomb rövid és hosszú megnyomását.
 * @param now Az aktuális idő milliszekundumban.
 * @return Igaz, ha üzemmódváltás történt, ezért a mérési ciklust ki kell hagyni.
 */
static bool handleZeroButton(uint32_t now) {

    // Ha a gombot most láttuk először nyomva, rögzítsük az időpontot és állítsuk a hosszú nyomás jelzőt hamisra.
    if (g_zeroSeen && !st.zeroPressStart) {
        st.zeroPressStart = now;
        st.zeroLong = false;
    }

    // Ha a gomb rövid ideje van nyomva, és még nem telt el a hosszú nyomás ideje, ne csináljunk semmit.
    if (g_zeroSeen && !st.setupMode && !st.zeroLong && now - st.zeroPressStart > LONG_PRESS_MS) {
        st.zeroLong = true;
        st.setupMode = true;
        st.centerMode = false;
        st.centerHistoryMode = false;
        st.best = 1e9f;
        restartZeroing();
        st.confirmUntil = now + 150;
        return true;
    }

    // Ha a gomb hosszú megnyomás után felengedésre kerül, ne csináljunk semmit.
    if (!g_zeroSeen && st.zeroPressStart) {
        if (!st.zeroLong) {
            if (st.teachMode) {
                // A tanítóképernyőből a nullázógomb kilép, újranullázás nélkül.
                st.teachMode = false;
                st.teachRequest = false;
                st.confirmUntil = now + 150;
            } else {
                if (st.setupMode) {
                    st.setupMode = false;
                    st.confirmUntil = now + 150;
                }
                restartZeroing();
            }
        }
        st.zeroPressStart = 0;
    }

    return false;
}

/**
 * @brief Rögzíti a kalibrálógomb állapotát, és kezeli a rövid és hosszú nyomást.
 * @param now Az aktuális idő milliszekundumban.
 *
 * Rövid nyomás: középkereső módváltás, tanítóképernyőn az 5 cm-es érték rögzítése.
 * Hosszú nyomás: belépés a tanítóképernyőre, illetve kilépés belőle.
 */
static void handleCalButton(uint32_t now) {
    if (g_calSeen && !st.calPressStart) {
        st.calPressStart = now;
        st.calDone = false;
        if (st.setupMode) {
            st.best = 1e9f; // A minimum érték inicializálása a beállító módban.
        }
    }

    const bool ready = !st.setupMode && st.zeroCount >= ZERO_UPDATES;

    if (g_calSeen && st.calPressStart && !st.calDone && ready && now - st.calPressStart >= LONG_PRESS_MS) {
        st.calDone = true;
        st.teachMode = !st.teachMode;
        st.teachRequest = false;
        st.centerMode = false;
        st.centerHistoryMode = false;
        st.flashUntil = 0;
        st.teachFailUntil = 0;
        st.confirmUntil = now + 150;
    }

    if (!g_calSeen && st.calPressStart) {
        if (!st.calDone && ready && now - st.calPressStart < LONG_PRESS_MS) {
            if (st.teachMode) {
                st.teachRequest = true;
            } else if (!st.centerMode) {
                st.centerMode = true;
                st.centerHistoryMode = false;
                st.centerFinder.reset();
                st.centerHistoryCount = 0;
                st.centerHistoryNext = 0;
                st.flashUntil = 0;
            } else if (!st.centerHistoryMode) {
                st.centerHistoryMode = true;
            } else {
                st.centerMode = false;
                st.centerHistoryMode = false;
            }
            st.confirmUntil = now + 150;
        }
        st.calPressStart = 0;
    }
}

/**
 * @brief A DD-beállító mód áthallásához hangfrekvenciát rendel.
 * @param a Az áthallás abszolút értéke.
 * @return A hangjelző frekvenciája hertzben.
 */
static uint16_t setupFreq(float a) {

    // Számítsuk ki a jel abszolút értékének arányát a teljes skálához képest.
    const float frac = constrain(log(1.0f + a) / log(1.0f + SETUP_FULL), 0.0f, 1.0f);

    // A frekvencia a 300 Hz és 3000 Hz közötti tartományban változik a jel arányától függően.
    return 300 + (uint16_t)(frac * 2700);
}

/**
 * @brief Frissíti a DD-beállító mód minimumát és hangjelzését.
 * @param m Az aktuális mérési eredmény.
 * @param out A mód által frissítendő hang- és kijelzési adatok.
 *
 */
static void runSetupMode(const Measurement &m, Outcome &out) {

    // Számítsuk ki az aktuális jel abszolút értékét.
    const float a = fabsf(m.diff);

    // Ha túlvezérlés van, állítsuk be a figyelmeztető frekvenciát és lépjünk ki.
    if (m.overload) {
        out.freq = 250;
        return;
    }

    // Frissítjük a legjobb (minimum) értéket, ha az aktuális jel kisebb.
    if (a < st.best) {
        st.best = a;
    }

    // Ha a jel eléri a detektálási küszöböt, állítsuk be a hangfrekvenciát.
    if (a >= DETECT_TH) {
        out.freq = setupFreq(a);
    }
}

/**
 * @brief Összegyűjti a nullázási mintákat, és meghatározza a zajküszöböt.
 * @param m Az aktuális mérési eredmény.
 * @param out A nullázási állapotüzenet kimenete.
 *
 */
static void runZeroing(const Measurement &m, Outcome &out) {

    // Ha ez az első minta, nullázzuk a négyzetösszeget is.
    if (st.zeroCount == 0) {
        st.zeroSq = 0;
        st.zeroS1Acc = 0;
        st.zeroS2Acc = 0;
        st.zeroRawDiffAcc = 0;
    }

    st.zeroAcc += m.diff;
    st.zeroSq += m.diff * m.diff;
    st.zeroS1Acc += m.s1Mean;
    st.zeroS2Acc += m.s2Mean;
    st.zeroRawDiffAcc += m.rawDiff;
    st.zeroCount++;

    // Ha elértük a szükséges mintaszámot, számítsuk ki a nullát és a zajküszöböt.
    if (st.zeroCount == ZERO_UPDATES) {
        st.zero = st.zeroAcc / ZERO_UPDATES;
        st.baselineS1 = st.zeroS1Acc / ZERO_UPDATES;
        st.baselineS2 = st.zeroS2Acc / ZERO_UPDATES;
        st.baselineDiff = st.zeroRawDiffAcc / ZERO_UPDATES;
        const float var = st.zeroSq / ZERO_UPDATES - st.zero * st.zero;
        st.runTh = max(DETECT_TH, NOISE_TH_FACTOR * sqrtf(max(var, 0.0f)));
        st.detOn = false;
    }
    out.msg = "Nullazas - legben!";
}

/**
 * @brief Elvégzi a gombnyomásra indított mélységkalibrálást.
 * @param m Az aktuális mérési eredmény.
 * @param now Az aktuális idő milliszekundumban.
 * @param out A kalibrálás kijelzési és hangjelzési adatai.
 *
 */
static void handleCalibration(const Measurement &m, uint32_t now, Outcome &out) {
    if (!st.teachMode) {
        return;
    }

    // A tanítás a nyers jelből dolgozik, hogy az előjelzár ne befolyásolja.
    const float teachMag = fabsf(m.diff - st.zero);

    if (st.teachRequest) {
        st.teachRequest = false;
        if (!m.overload && teachMag >= st.runTh) {
            calSet(calKFromSignal(teachMag));
            st.confirmUntil = now + 300;
            st.flashUntil = now + 1500;
            st.teachFailUntil = 0;
        } else {
            st.teachFailUntil = now + 1500;
            st.flashUntil = 0;
        }
    }

    if (now < st.flashUntil) {
        out.msg = "Rogzitve: 5 cm";
    } else if (now < st.teachFailUntil) {
        out.msg = m.overload ? "Tulvezerles!" : "Nincs jel!";
    }
}

/**
 * @brief Kiszámítja a nullázott jelet, és frissíti a találat állapotát.
 * @param m Az aktuális mérési eredmény.
 * @param now Az aktuális idő milliszekundumban.
 * @param out A találat, a hang és a kijelzés kimeneti adatai.
 *
 */
static void runDetect(const Measurement &m, uint32_t now, Outcome &out) {
    out.sig = m.diff - st.zero;
    const float rawSig = out.sig;
    const float rawMag = fabsf(rawSig);

    // A találat előjelét a stabil alapvonal-visszatérésig megtartjuk.
    if (st.signalPolarity == 0 && rawMag >= st.runTh) {
        st.signalPolarity = rawSig < 0 ? -1 : 1;
        st.recoveryActive = true;
        st.recoveryCount = 0;
        st.recoveryS1Acc = 0;
        st.recoveryS2Acc = 0;
        st.recoveryDiffAcc = 0;
    }

    if (st.signalPolarity != 0 && rawSig * st.signalPolarity < 0) {
        out.sig = 0;
    }

    bool recovered = false;
    if (st.recoveryActive && !st.centerMode) {
        const float channelTolerance = max(2.0f, 2.0f * st.runTh);
        const bool baselineReturned =
            !m.overload && !m.dcFault && fabsf(m.rawDiff - st.baselineDiff) < 0.5f * st.runTh && fabsf(m.s1Mean - st.baselineS1) < channelTolerance && fabsf(m.s2Mean - st.baselineS2) < channelTolerance;

        if (baselineReturned) {
            st.recoveryS1Acc += m.s1Mean;
            st.recoveryS2Acc += m.s2Mean;
            st.recoveryDiffAcc += m.rawDiff;
            if (st.recoveryCount < RECOVERY_STABLE_SAMPLES) {
                ++st.recoveryCount;
            }
        } else {
            st.recoveryCount = 0;
            st.recoveryS1Acc = 0;
            st.recoveryS2Acc = 0;
            st.recoveryDiffAcc = 0;
        }

        if (st.recoveryCount >= RECOVERY_STABLE_SAMPLES) {
            const float sampleCount = (float)st.recoveryCount;
            st.baselineS1 = st.recoveryS1Acc / sampleCount;
            st.baselineS2 = st.recoveryS2Acc / sampleCount;
            st.baselineDiff = st.recoveryDiffAcc / sampleCount;
            st.zero = st.baselineDiff;
            st.smooth = st.zero;
            st.signalPolarity = 0;
            st.recoveryActive = false;
            st.recoveryCount = 0;
            st.recoveryS1Acc = 0;
            st.recoveryS2Acc = 0;
            st.recoveryDiffAcc = 0;
            st.detOn = false;
            out.sig = 0;
            recovered = true;
        }
    }

    out.mag = fabsf(out.sig);
    if (!st.centerMode && !st.teachMode && !st.recoveryActive && !recovered && out.mag < st.runTh) {
        st.zero += DRIFT_FOLLOW * out.sig;
        st.baselineS1 += DRIFT_FOLLOW * (m.s1Mean - st.baselineS1);
        st.baselineS2 += DRIFT_FOLLOW * (m.s2Mean - st.baselineS2);
        st.baselineDiff += DRIFT_FOLLOW * (m.rawDiff - st.baselineDiff);
    }
    if (!recovered) {
        st.detOn = out.mag >= (st.detOn ? DETECT_RELEASE_FRACTION * st.runTh : st.runTh);
    }
    // Frissítjük a detektor kimeneti állapotát a jel nagysága és az érzékelő túlterheltsége alapján.
    out.det = st.detOn && !m.overload;

    // Kezeljük a kalibrációt, ha szükséges.
    handleCalibration(m, now, out);

    if (out.det) {
        // Kiszámítjuk a hangfrekvenciát a jel nagysága alapján.
        out.freq = 800 + (uint16_t)(constrain(out.mag / calFullScale(), 0.0f, 1.0f) * 2200);
    }
}

/**
 * @brief Frissíti a hangjelző állapotát és frekvenciáját.
 * @param freq A kért hangfrekvencia hertzben; nulla esetén a hangjelző kikapcsol.
 * @param now Az aktuális idő milliszekundumban.
 *
 */
static void updateBeeper(uint16_t freq, uint32_t now) {

    if (now < st.confirmUntil) {
        freq = 3000;
    }

    if (freq) {
        beepOn(freq);
    } else {
        beepOff();
    }
}

/**
 * @brief Kiírja a mérési eredményeket a soros portra.
 * @param m Az aktuális mérési eredmény.
 * @param out Az üzemmód kimeneti adatai.
 *
 */
#if defined(__DEBUG__) && defined(__DEBUG_SERIAL_LOGGING__)
static void logSerial(const Measurement &m, const Outcome &out) {

    Serial.print(st.setupMode ? F("setup") : (st.centerMode ? F("center") : F("run")));
    if (st.setupMode) {
        Serial.print(F(" ch=A"));
        Serial.print(CHANNEL);
    }

    Serial.print(F(" diff=")); // Különbség a mérés és a nulla között
    Serial.print(m.diff, 2);

    Serial.print(F(" raw_diff="));
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

    if (st.centerMode) {
        Serial.print(F(" center_sig="));
        Serial.print(m.diff - st.zero, 2);
        Serial.print(F(" noise_th="));
        Serial.print(st.runTh, 2);
        Serial.print(F(" center_state="));
        Serial.print(centerStageLabel(st.centerFinder.stage()));
        Serial.print(F(" peak="));
        Serial.print(st.centerFinder.peakMagnitude(), 2);
        Serial.print(F(" return_peak="));
        Serial.print(st.centerFinder.returnMagnitude(), 2);
        Serial.print(F(" peak_sample="));
        Serial.print(st.centerFinder.peakSample());
        Serial.print(F(" return_sample="));
        Serial.print(st.centerFinder.returnSample());
    }

    Serial.println();
}
#endif

/**
 * @brief Elindítja a soros portot, az érzékelőt, a hangjelzőt és a kijelzőt.
 *
 */
void setup() {

#ifdef __DEBUG__
#ifdef __DEBUG_SERIAL_LOGGING__
    Serial.begin(115200);
#else
    debug_init();
#endif
#endif

    sensorInit();  // érzékelő inicializálása
    beepInit();    // hangjelző inicializálása
    calLoad();     // kalibrációs adatok betöltése
    displayInit(); // kijelző inicializálása

#if USE_WATCHDOG
    wdt_enable(WDTO_1S); // watchdog engedélyezése 1 másodperces időzítővel
#endif
}

/**
 * @brief Végrehajtja a mérési főciklust és frissíti a kijelzőket.
 *
 */
void loop() {
    const Measurement m = acquire();
    const uint32_t now = millis();

    // Kezeljük a nulla gombot.
    if (handleZeroButton(now)) {
        return;
    }
    // Kezeljük a kalibrációs gombot.
    handleCalButton(now);

    Outcome out;
    if (st.setupMode) {
        runSetupMode(m, out); // Futtatjuk a beállítási üzemmódot.
    } else if (st.zeroCount < ZERO_UPDATES) {
        runZeroing(m, out); // Futtatjuk a nulla beállítási üzemmódot.
    } else {
        runDetect(m, now, out); // Egyébként a detektálási üzemmódot.
        if (st.centerMode) {
            const float centerSig = m.diff - st.zero;
            const bool newlyLocked = st.centerFinder.update(fabsf(centerSig), st.runTh, !m.overload && !m.dcFault, now);
            if (newlyLocked) {
                st.confirmUntil = now + 150;
            }

            const float historyScale = st.runTh * 4.0f;
            int16_t historyValue = historyScale > 0 ? (int16_t)(fabsf(centerSig) * 32.0f / historyScale) : 0;
            historyValue = constrain(historyValue, 0, 32);
            st.centerHistory[st.centerHistoryNext] = (int8_t)historyValue;
            st.centerHistoryNext = (st.centerHistoryNext + 1) % GRAPH_HISTORY_SIZE;
            if (st.centerHistoryCount < GRAPH_HISTORY_SIZE) {
                ++st.centerHistoryCount;
            }
        }
    }

    updateBeeper(out.freq, now); // Frissítjük a hangjelző állapotát és frekvenciáját.

    if (st.setupMode) {
        drawSetup(m.diff, st.best, m.s1Mean, m.overload, m.dcFault, m.vbat);
    } else if (st.teachMode && st.zeroCount >= ZERO_UPDATES) {
        drawTeach(out.msg, fabsf(m.diff - st.zero), st.runTh, m.vbat, m.overload);
    } else if (st.centerMode && st.zeroCount >= ZERO_UPDATES) {
        drawCenter(out.msg, m.diff - st.zero, st.runTh, st.centerFinder.peakMagnitude(), st.centerFinder.returnMagnitude(), m.vbat, m.overload, m.dcFault, st.centerHistoryMode, st.centerFinder.stage(), st.centerHistory,
                   st.centerHistoryCount, st.centerHistoryNext);
    } else {
        drawNormal(out.msg, out.mag, m.vbat, m.overload, out.det);
    }

#if defined(__DEBUG__) && defined(__DEBUG_SERIAL_LOGGING__)
    // 500 milliszekundumonként naplózzuk a soros porton.
    static long lastLog = 0;
    if (timeHasPassed(lastLog, 500)) {
        logSerial(m, out);
        lastLog = millis();
    }
#endif
}
