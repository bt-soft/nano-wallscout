/**
 * @file Arduino Nano-alapú, DD-tekercses betonvas-kereső vezérlőprogramja.
 *
 * Az impulzusmérés egyetlen szálon fut; a kritikus mintavételezés alatt
 * a megszakítások tiltva vannak. A Timer1 időalapja 0,5 µs, a hangjelzőt
 * a D11-es Timer2-kimenet vezérli.
 *
 * A konfigurációt a config.h, a mérést a sensor, a hangjelzést a beeper,
 * a mélységkalibrálást a calibration, a kijelzést pedig a display kezeli.
 */

#include <Arduino.h>
#include <avr/wdt.h>
#include <math.h>

#include "beeper.h"
#include "calibration.h"
#include "config.h"
#include "display.h"
#include "sensor.h"

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

    /** Az aktuális találat előjele; ellenkező előjelnél a jel el van nyomva. */
    int8_t signalPolarity = 0;

    /** A küszöb alatti állapot kezdete az előjelzár feloldásához. */
    uint32_t polarityQuietSince = 0;

    /** Az előző simított különbség és az egymás utáni állandó mérések száma. */
    float prevDiff = 0;
    uint16_t steadyCount = 0;

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

    // A simított különbség kiszámítása az S1 és S2 átlagok alapján.
    m.diff = smoothDiff((float)((int32_t)s1 - (int32_t)s2) / N_PULSES);

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
    st.polarityQuietSince = 0;
    st.steadyCount = 0;
    st.detOn = false;
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
        st.best = 1e9f;
        restartZeroing();
        st.confirmUntil = now + 150;
        return true;
    }

    // Ha a gomb hosszú megnyomás után felengedésre kerül, ne csináljunk semmit.
    if (!g_zeroSeen && st.zeroPressStart) {
        if (!st.zeroLong) {
            if (st.setupMode) {
                st.setupMode = false;
                st.confirmUntil = now + 150;
            }
            restartZeroing();
        }
        st.zeroPressStart = 0;
    }

    return false;
}

/**
 * @brief Rögzíti a kalibrálógomb állapotát, és törli a beállító mód minimumát.
 * @param now Az aktuális idő milliszekundumban.
 *
 */
static void handleCalButton(uint32_t now) {
    if (g_calSeen && !st.calPressStart) {
        st.calPressStart = now;
        st.calDone = false;
        if (st.setupMode) {
            st.best = 1e9f; // A minimum érték inicializálása a beállító módban.
        }
    }

    if (!g_calSeen) {
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
    }

    st.zeroAcc += m.diff;
    st.zeroSq += m.diff * m.diff;
    st.zeroCount++;

    // Ha elértük a szükséges mintaszámot, számítsuk ki a nullát és a zajküszöböt.
    if (st.zeroCount == ZERO_UPDATES) {
        st.zero = st.zeroAcc / ZERO_UPDATES;
        const float var = st.zeroSq / ZERO_UPDATES - st.zero * st.zero;
        st.runTh = max(DETECT_TH, 5.0f * sqrtf(max(var, 0.0f)));
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

    const bool calHeld = g_calSeen && st.calPressStart && !st.calDone;
    const bool longPress = now - st.calPressStart > LONG_PRESS_MS;

    if (calHeld && longPress && out.det) {
        calSet(calKFromSignal(out.mag));
        st.calDone = true;
        st.confirmUntil = now + 300;
        st.flashUntil = now + 1500;
    } else if (calHeld) {
        out.msg = longPress ? (m.overload ? "Tulvezerles!" : "Nincs jel! Vas 5cm") : "Kalibralas...";

    } else if (now < st.flashUntil) {
        out.msg = "Kalibralva (5 cm)";
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

    // Ha még nincs jel polaritása, és a jel eléri a küszöböt, állítsuk be a polaritást.
    if (st.signalPolarity == 0 && rawMag >= st.runTh) {
        st.signalPolarity = rawSig < 0 ? -1 : 1;
    }

    // Ha van jel polaritása, ellenőrizzük, hogy a jel csendes-e a polaritás visszaállításához.
    if (st.signalPolarity != 0) {

        // Ha a jel csendes a polaritás visszaállításához, kezdjük el a számlálást.
        if (rawMag < st.runTh) {
            if (!st.polarityQuietSince)
                st.polarityQuietSince = now;
            if (now - st.polarityQuietSince >= POLARITY_RELEASE_MS) {
                st.signalPolarity = 0;
                st.polarityQuietSince = 0;
            }
        } else {
            st.polarityQuietSince = 0;
        }
        if (st.signalPolarity != 0 && rawSig * st.signalPolarity < 0)
            out.sig = 0;
    }

    // Hosszan álló jel nem mozgó fém, hanem nullpont-eltolódás: lassan újranullázunk.
    const bool steady = fabsf(m.diff - st.prevDiff) < 0.3f * st.runTh;
    st.prevDiff = m.diff;
    if (steady) {
        if (st.steadyCount < 255)
            st.steadyCount++;
    } else {
        st.steadyCount = 0;
    }

    // Ellenőrizzük, hogy a jel hosszú ideje áll-e, ami a nullpont-eltolódás jele lehet.
    const bool rezero = st.steadyCount >= STEADY_CYCLES;
    if (rezero) {
        st.zero += REZERO_FOLLOW * rawSig;
        out.sig = m.diff - st.zero;
        if (fabsf(out.sig) < st.runTh)
            st.signalPolarity = 0;
    }

    // Frissítjük a jel nagyságát a kimeneti adatokban.
    out.mag = fabsf(out.sig);
    // Ha a jel nem hosszú ideje áll, és a jel nagysága a küszöb alatt van, követjük a driftet.
    if (!rezero && out.mag < st.runTh) {
        st.zero += DRIFT_FOLLOW * out.sig;
    }
    // Frissítjük a detektor állapotát a jel nagysága alapján.
    st.detOn = out.mag >= (st.detOn ? 0.6f * st.runTh : st.runTh);
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

#ifdef __DEBUG__

/**
 * @brief Kiírja a mérési eredményeket a soros portra.
 * @param m Az aktuális mérési eredmény.
 * @param out Az üzemmód kimeneti adatai.
 *
 */
static void logSerial(const Measurement &m, const Outcome &out) {

    Serial.print(st.setupMode ? F("setup") : F("run"));
    if (st.setupMode) {
        Serial.print(F(" ch=A"));
        Serial.print(CHANNEL);
    }

    Serial.print(F(" diff=")); // Különbség a mérés és a nulla között
    Serial.print(m.diff, 2);

    Serial.print(F(" s1=")); // S1 érzékelő átlagértéke
    Serial.print(m.s1Mean, 0);

    Serial.print(F(" s2=")); // S2 érzékelő átlagértéke
    Serial.print(m.s2Mean, 0);

    Serial.print(F(" idle=")); // Inaktív állapot mérése
    Serial.print(m.idle);

    Serial.print(F(" zero=")); // Nulla érték
    Serial.print(st.zero, 2);

    Serial.print(F(" sig=")); // Jelszint
    Serial.print(out.sig, 2);

    Serial.print(F(" vbat=")); // Akkumulátor feszültség
    Serial.println(m.vbat, 2);
}
#endif

/**
 * @brief Elindítja a soros portot, az érzékelőt, a hangjelzőt és a kijelzőt.
 *
 */
void setup() {

#ifdef __DEBUG__
    Serial.begin(115200);
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
    }

    updateBeeper(out.freq, now); // Frissítjük a hangjelző állapotát és frekvenciáját.

    if (st.setupMode) {
        drawSetup(m.diff, st.best, m.s1Mean, m.overload, m.dcFault, m.vbat);
    } else {
        drawNormal(out.msg, out.mag, m.vbat, m.overload, out.det);
    }
#ifdef __DEBUG__
    logSerial(m, out);
#endif
}
