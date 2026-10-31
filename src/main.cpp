/**
 * @file Arduino Nano-alapú, DD-tekercses betonvas-kereső vezérlőprogramja.
 *
 * Az impulzusmérés egyetlen szálon fut; a kritikus mintavételezés alatt
 * a megszakítások tiltva vannak.
 * A Timer1 időalapja 0,5 µs, a hangjelzőt a D3-as Timer2-kimenet (OC2B) vezérli.
 *
 * Képernyők (kalibrálógomb, rövid nyomás):
 *  - ENABLE_CENTER_FINDER_SCREEN = 1: Keresés → Középkereső → Jel-Ido → Keresés
 *  - ENABLE_CENTER_FINDER = 0: Keresés → Jel-Ido → Keresés
 * A tanító és a DD-beállító mód hosszú gombnyomással érhető el.
 */

#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <math.h>

#include "app_types.h"
#include "beeper.h"
#include "calibration.h"
#include "config.h"
#include "display.h"
#include "operating_mode.h"
#include "sensor.h"
#include "serial_log.h"
#include "utils.h"

/**
 * @brief A program globális mérési és üzemmód-állapota.
 */
static State st;

// ---------------------------------------------------------------------------
// Üzemmódok
// ---------------------------------------------------------------------------

/**
 * @brief Igaz, ha grafikus nézet (Jel-Ido vagy középkereső) aktív;
 * ezekben mindkét előjelű jel számít.
 */
static bool isGraphMode(OperatingMode mode) { return mode == OperatingMode::CENTER_FINDER || mode == OperatingMode::SIGNAL_TIME; }

/**
 * @brief A kalibrálógomb rövid nyomására következő képernyő a körforgásban.
 */
static OperatingMode nextScreen(OperatingMode mode) {
    switch (mode) {
#if ENABLE_CENTER_FINDER_SCREEN
        case OperatingMode::SEARCH:
            return OperatingMode::CENTER_FINDER;
        case OperatingMode::CENTER_FINDER:
            return OperatingMode::SIGNAL_TIME;
#else
        case OperatingMode::SEARCH:
            return OperatingMode::SIGNAL_TIME;
#endif
        default:
            return OperatingMode::SEARCH;
    }
}

/**
 * @brief Törli a grafikon és a középkereső állapotát.
 */
static void resetGraphs() {
    st.graph.reset();
#if ENABLE_CENTER_FINDER_SCREEN
    st.centerFinder.reset();
#endif
}

// ---------------------------------------------------------------------------
// Mérés
// ---------------------------------------------------------------------------

/**
 * @brief Simítja az S1 − S2 különbséget exponenciális átlagolással.
 *
 * @param diff Az új, simítandó mérési különbség.
 * @return A simított különbség.
 */
static float smoothDiff(float diff) {

    // Ha a csatorna megváltozott, vagy a különbség hirtelen ugrik, állítsuk vissza a simított értéket.
    if (CHANNEL != st.smoothCh || fabsf(diff - st.smooth) > SMOOTH_JUMP) {
        st.smooth = diff;
        st.smoothCh = CHANNEL;
    } else {
        st.smooth += (st.mode == OperatingMode::DD_SETUP ? SMOOTH_SETUP : SMOOTH_RUN) * (diff - st.smooth);
    }

    return st.smooth;
}

/**
 * @brief Elvégzi az impulzusmérést, és kiszámítja a mérés állapotjelzőit.
 *
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

    m.s1Mean = (float)s1 / N_PULSES;
    m.s2Mean = (float)s2 / N_PULSES;
    m.rawDiff = (float)((int32_t)s1 - (int32_t)s2) / N_PULSES;
    m.diff = smoothDiff(m.rawDiff);

    m.overload = m.s1Mean > OVERLOAD_HIGH || m.s1Mean < OVERLOAD_LOW;
    m.dcFault = m.idle > IDLE_HIGH || m.idle < IDLE_LOW;

    return m;
}

// ---------------------------------------------------------------------------
// Gombok
// ---------------------------------------------------------------------------

/**
 * @brief Újrakezdi az indulási vagy kézi nullázás mintagyűjtését.
 */
static void restartZeroing() {
    st.zeroCount = 0;
    st.zeroAcc = 0;
    st.detOn = false;
    resetGraphs();
    if (st.mode == OperatingMode::TEACH) {
        st.mode = OperatingMode::SEARCH;
    }
    st.teachRequest = false;

    // A simított különbség és a csatorna inicializálása.
    st.smooth = 0;
    st.smoothCh = 255;
}

/**
 * @brief Kezeli a nullázógomb rövid és hosszú megnyomását.
 *
 * @param now Az aktuális idő milliszekundumban.
 * @return Igaz, ha üzemmódváltás történt, ezért a mérési ciklust ki kell hagyni.
 */
static bool handleZeroButton(uint32_t now) {

    // Ha a gombot most láttuk először nyomva, rögzítsük az időpontot.
    if (g_zeroSeen && !st.zeroPressStart) {
        st.zeroPressStart = now;
        st.zeroLong = false;
    }

    // Hosszú nyomás: belépés a DD-beállító módba.
    if (g_zeroSeen && st.mode != OperatingMode::DD_SETUP && !st.zeroLong && now - st.zeroPressStart > LONG_PRESS_MS) {
        st.zeroLong = true;
        st.mode = OperatingMode::DD_SETUP;
        st.best = FLT_MAX;
        restartZeroing();
        st.confirmUntil = now + 150;
        return true;
    }

    // Rövid nyomás felengedése.
    if (!g_zeroSeen && st.zeroPressStart) {
        if (!st.zeroLong) {
            if (st.mode == OperatingMode::TEACH) {
                // A tanítóképernyőből a nullázógomb kilép, újranullázás nélkül.
                st.mode = OperatingMode::SEARCH;
                st.teachRequest = false;
                st.confirmUntil = now + 150;
            } else {
                if (st.mode == OperatingMode::DD_SETUP) {
                    st.mode = OperatingMode::SEARCH;
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
 *
 * @param now Az aktuális idő milliszekundumban.
 *
 * Rövid nyomás: következő képernyő, tanítóképernyőn az 5 cm-es érték rögzítése.
 * Hosszú nyomás: belépés a tanítóképernyőre, illetve kilépés belőle.
 */
static void handleCalButton(uint32_t now) {

    // Ha a gombot most láttuk először nyomva, rögzítsük az időpontot.
    if (g_calSeen && !st.calPressStart) {
        st.calPressStart = now;
        st.calDone = false;
        if (st.mode == OperatingMode::DD_SETUP) {
            st.best = FLT_MAX; // A minimum érték inicializálása a beállító módban.
        }
    }

    const bool ready = st.mode != OperatingMode::DD_SETUP && st.zeroCount >= ZERO_UPDATES;

    // Hosszú nyomás: belépés a tanító módba, illetve kilépés belőle.
    if (g_calSeen && st.calPressStart && !st.calDone && ready && now - st.calPressStart >= LONG_PRESS_MS) {
        st.calDone = true;
        st.mode = st.mode == OperatingMode::TEACH ? OperatingMode::SEARCH : OperatingMode::TEACH;
        st.teachRequest = false;
        st.flashUntil = 0;
        st.teachFailUntil = 0;
        st.confirmUntil = now + 150;
    }

    // Rövid nyomás felengedése: rögzítés a tanító módban, egyébként képernyőváltás.
    if (!g_calSeen && st.calPressStart) {
        if (!st.calDone && ready && now - st.calPressStart < LONG_PRESS_MS) {
            if (st.mode == OperatingMode::TEACH) {
                st.teachRequest = true;
            } else {
                if (st.mode == OperatingMode::SEARCH) {
                    resetGraphs();
                    st.flashUntil = 0;
                }
                st.mode = nextScreen(st.mode);
            }
            st.confirmUntil = now + 150;
        }
        st.calPressStart = 0;
    }
}

// ---------------------------------------------------------------------------
// Jelfeldolgozás
// ---------------------------------------------------------------------------

/**
 * @brief A DD-beállító mód áthallásához hangfrekvenciát rendel (300–3000 Hz).
 *
 * @param a Az áthallás abszolút értéke.
 * @return A hangjelző frekvenciája hertzben.
 */
static uint16_t setupFreq(float a) { return 300 + (uint16_t)(setupFraction(a) * 2700); }

/**
 * @brief Frissíti a DD-beállító mód minimumát és hangjelzését.
 *
 * @param m Az aktuális mérési eredmény.
 * @param out A mód által frissítendő hang- és kijelzési adatok.
 */
static void runSetupMode(const Measurement &m, Outcome &out) {
    const float a = fabsf(m.diff);

    if (m.overload) {
        out.freq = 250;
        return;
    }

    if (a < st.best) {
        st.best = a;
    }

    if (a >= DETECT_TH) {
        out.freq = setupFreq(a);
    }
}

/**
 * @brief Összegyűjti a nullázási mintákat, és meghatározza a zajküszöböt.
 *
 * @param m Az aktuális mérési eredmény.
 * @param out A nullázási állapotüzenet kimenete.
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
        st.runTh = max(DETECT_TH, NOISE_TH_FACTOR * sqrtf(max(var, 0.0f)));
        st.detOn = false;
    }

    out.msg = "Nullazas...";
}

/**
 * @brief Elvégzi a gombnyomásra indított mélységkalibrálást a tanító módban.
 *
 * @param m Az aktuális mérési eredmény.
 * @param now Az aktuális idő milliszekundumban.
 * @param out A kalibrálás kijelzési adatai.
 */
static void handleCalibration(const Measurement &m, uint32_t now, Outcome &out) {
    if (st.mode != OperatingMode::TEACH) {
        return;
    }

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
 *
 * @param m Az aktuális mérési eredmény.
 * @param now Az aktuális idő milliszekundumban.
 * @param out A találat, a hang és a kijelzés kimeneti adatai.
 */
static void runDetect(const Measurement &m, uint32_t now, Outcome &out) {
    out.sig = m.diff - st.zero;
    out.mag = fabsf(out.sig);

    if (st.mode == OperatingMode::SEARCH) {
        // A nulla csak találat nélkül, a küszöb alatti jelnél követi a driftet,
        // így a tekercsen tartott vasat nem nyeli el.
        if (!st.detOn && out.mag < DETECT_RELEASE_FRACTION * st.runTh) {
            st.zero += DRIFT_FOLLOW * out.sig;
        }
    } else if (isGraphMode(st.mode) && st.detOn && out.mag < st.runTh) {
        // A grafikus módokban a nulla nem követi a driftet, ezért egy elmozdult alapvonal a
        // hiszterézis-sávban (release..runTh) bent tartaná a találatot. Csak ebben a sávban,
        // lassan követjük a nullát.
        st.zero += (DRIFT_FOLLOW * 0.25f) * out.sig;
    }

    st.detOn = out.mag >= (st.detOn ? DETECT_RELEASE_FRACTION * st.runTh : st.runTh);
    out.det = st.detOn && !m.overload;

    handleCalibration(m, now, out);

    // Hangfrekvencia a jel nagysága alapján (800–3000 Hz).
    if (out.det) {
        out.freq = 800 + (uint16_t)(constrain(out.mag / calFullScale(), 0.0f, 1.0f) * 2200);
    }
}

/**
 * @brief Grafikus módokban frissíti a gördülő grafikont és a középkeresőt.
 *
 * @param m Az aktuális mérési eredmény.
 * @param out A detektor kimenete (az előjeles jellel).
 * @param now Az aktuális idő milliszekundumban.
 */
static void updateGraphs(const Measurement &m, const Outcome &out, uint32_t now) {
    if (!isGraphMode(st.mode)) {
        return;
    }

#if ENABLE_CENTER_FINDER_SCREEN
    if (st.centerFinder.update(out.mag, st.runTh, !m.overload && !m.dcFault, now)) {
        st.confirmUntil = now + 150; // Új középpont: hangjelzés
    }
#endif

    // A grafikon függőleges skálája: a küszöb négyszerese = 32 egység.
    st.graph.push((int8_t)constrain((int16_t)(out.mag * 8.0f / st.runTh), 0, 32));
}

/**
 * @brief Frissíti a hangjelző állapotát és frekvenciáját.
 *
 * @param freq A kért hangfrekvencia hertzben; nulla esetén a hangjelző kikapcsol.
 * @param now Az aktuális idő milliszekundumban.
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
 * @brief Az aktuális üzemmódnak megfelelő feldolgozás: beállítás, nullázás vagy detektálás.
 */
static Outcome process(const Measurement &m, uint32_t now) {
    Outcome out;

    if (st.mode == OperatingMode::DD_SETUP) {
        runSetupMode(m, out);
    } else if (st.zeroCount < ZERO_UPDATES) {
        runZeroing(m, out);
    } else {
        runDetect(m, now, out);
        updateGraphs(m, out, now);
    }

    return out;
}

// ---------------------------------------------------------------------------
// Megjelenítés
// ---------------------------------------------------------------------------

/**
 * @brief Kirajzolja az aktuális üzemmód képernyőjét.
 */
static void render(const Measurement &m, const Outcome &out) {
    if (st.mode == OperatingMode::DD_SETUP) {
        drawDDSetup(m.diff, st.best, m.s1Mean, m.overload, m.dcFault, m.vbat);
        return;
    }

    // Nullázás közben mindig a normál képernyő látszik (a "Nullazas..." üzenettel).
    if (st.zeroCount >= ZERO_UPDATES) {
        switch (st.mode) {
            case OperatingMode::TEACH:
                drawTeach(out.msg, out.mag, st.runTh, m.vbat, m.overload, out.sig < 0 ? -1 : 1);
                return;
#if ENABLE_CENTER_FINDER_SCREEN
            case OperatingMode::CENTER_FINDER:
                drawCenterFinder(out.msg, out.sig, st.runTh, st.centerFinder.peakMagnitude(), st.centerFinder.returnMagnitude(), m.vbat, m.overload, m.dcFault, st.centerFinder.stage(), st.graph);
                return;
#endif // ENABLE_CENTER_FINDER_SCREEN
            case OperatingMode::SIGNAL_TIME:
                drawSignalTime(out.sig, st.runTh, m.vbat, m.overload, m.dcFault, st.graph);
                return;
            default:
                break;
        }
    }

    drawNormal(out.msg, out.mag, m.vbat, m.overload, out.det);
}

// ---------------------------------------------------------------------------
// PowerDown kezelése
// ---------------------------------------------------------------------------

/**
 * @brief Gombváltás ébresztő megszakítása (PD4/PD5);
 */
ISR(PCINT2_vect) {
    // Pin Change Interrupt kezelése (PD4/PD5)
    // Üres az ISR, csak az alvásból ébreszt.
}

// Szundi változók
static uint32_t lastActivity = 0;     // Utolsó aktivitás ideje milliszekundumban a LowPower állapothoz
static float activityRef = 0;         // Aktivitás referenciaértéke
static bool activityRefValid = false; // Aktivitás referencia érvényessége

/**
 * @brief Mély alvó módba kapcsol; gombnyomásra ébred, majd újraindítja a mérést.
 *
 * Kikapcsolja a tápot (kapu), a hangjelzőt, a kijelzőt, az ADC-t és minden perifériát,
 * a BOD-ot (Brown-Out Detector) és a watchdogot, majd Power-down alvásba lép.
 */
static void enterDeepSleep() {
    beepOff();               // Hangjelző kikapcsolása
    PORTD &= ~_BV(GATE_BIT); // Adó kikapcsolva
    displaySleep();          // Kijelző alvó módba kapcsolása

    wdt_disable(); // Watchdog letiltása
    ADCSRA = 0;    // ADC letiltása
    DIDR0 = 0x3F;  // ADC0-ADC5 bemenetek letiltása
    DIDR1 = 0x03;  // ADC6-ADC7 bemenetek letiltása

    PCIFR = _BV(PCIF2);                   // Pin Change Interrupt Flag 2 törlése
    PCMSK2 = _BV(PCINT20) | _BV(PCINT21); // Pin Change Mask beállítása PD4/PD5-re
    PCICR |= _BV(PCIE2);                  // Pin Change Interrupt 2 engedélyezése

    set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Power-down alvás mód beállítása
    power_all_disable();                 // Minden periféria letiltása
    cli();                               // Globális megszakítások letiltása
    sleep_enable();                      // Alvás engedélyezése
    sleep_bod_disable();                 // BOD letiltása alvás közben
    sei();                               // Globális megszakítások engedélyezése
    sleep_cpu();                         // CPU alvásba küldése
    sleep_disable();                     // Alvás letiltása az ébredés után

    PCICR &= ~_BV(PCIE2); // Pin Change Interrupt 2 letiltása
    PCMSK2 = 0;           // Pin Change Mask törlése
    power_all_enable();   // Minden periféria újra engedélyezése

    // Az ébresztő gomb felengedéséig várunk, hogy ne értelmeződjön gombnyomásként.
    delay(50);
    while (!(PIND & _BV(PIN_BTN_CAL)) || !(PIND & _BV(PIN_BTN_ZERO))) {
        // Várakozás valamelyik gomb felengedésére
    }
    delay(50);

    st = State();             // Állapot inicializálása
    activityRefValid = false; // Aktivitás referencia érvényességének visszaállítása
    sensorInit();             // Érzékelő inicializálása
    beepInit();               // Hangjelző inicializálása
    displayInit();            // Kijelző inicializálása
#if USE_WATCHDOG
    wdt_enable(WDTO_1S); // Watchdog engedélyezése 1 másodperces időzítővel
#endif
    lastActivity = millis(); // Utolsó aktivitás idejének beállítása az aktuális időre
}

/**
 * @brief Követi az aktivitást, és alvásba lép, ha az IDLE_SLEEP_MS ideig nem volt.
 *
 * Aktivitás: gombnyomás, találat, vagy a zajszintet meghaladó jelváltozás.
 *
 * @return Igaz, ha az eszköz alvásba lépett (és felébredt), ezért a ciklust ki kell hagyni.
 */
static bool handleIdleSleep(const Measurement &m, uint32_t now) {

    // Az aktivitás küszöbértéke
    const float activityTh = max(DETECT_TH, st.runTh);

    // Ha az aktivitás referencia még nem érvényes, inicializáljuk azt az aktuális mérési különbséggel.
    if (!activityRefValid) {
        activityRef = m.diff;
        activityRefValid = true;
        lastActivity = now;
    }

    // Ellenőrizzük, hogy történt-e aktivitás: gombnyomás, találat, vagy a zajszintet meghaladó jelváltozás.
    if (g_zeroSeen || g_calSeen || st.detOn || fabsf(m.diff - activityRef) >= activityTh) {
        activityRef = m.diff;
        lastActivity = now;
    }

    // Ellenőrizzük, hogy aludni kell-e az inaktivitás miatt.
    if (now - lastActivity >= IDLE_SLEEP_MS) {
        enterDeepSleep();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Arduino belépési pontok
// ---------------------------------------------------------------------------

/**
 * @brief Elindítja a soros portot, az érzékelőt, a hangjelzőt és a kijelzőt.
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
 * @brief Végrehajtja a mérési főciklust és frissíti a kijelzőt.
 */
void loop() {
    const Measurement m = acquire();
    const uint32_t now = millis();

    if (handleIdleSleep(m, now) || handleZeroButton(now)) {
        return;
    }
    handleCalButton(now);

    const Outcome out = process(m, now);
    updateBeeper(out.freq, now);
    render(m, out);

#ifdef __DEBUG__
    // x milliszekundumonként naplózzuk a soros porton.
    static long lastLog = 0;
    if (timeHasPassed(lastLog, 200)) {
        logSerial(st, m, out);
        lastLog = millis();
    }
#endif
}
