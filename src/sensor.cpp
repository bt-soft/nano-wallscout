#include <avr/io.h>

#include "config.h"
#include "sensor.h"

bool g_zeroSeen = false, g_calSeen = false;

/**
 * @brief Elindít egy ADC-konverziót, majd megvárja az eredményt.
 * @return A konverzió eredménye ADC-egységben.
 */
static inline uint16_t adcRead() {
    ADCSRA |= _BV(ADSC);         // elindítja az ADC-konverziót
    while (ADCSRA & _BV(ADSC)) { // várakozik az ADC-konverzió befejezésére
    }
    return ADC; // visszaadja az ADC-konverzió eredményét
}

/**
 * @brief Gyors, 1 MHz-es ADC-órajelet állít be az előosztó 16-ra állításával.
 *
 */
static void adcFast() {
    // előosztó 16-ra állítva
    ADCSRA = _BV(ADEN) | _BV(ADPS2);
}

/**
 * @brief Lassú ADC-órajelet állít be az előosztó 128-ra állításával.
 *
 */
static void adcSlow() {
    // előosztó 128-ra állítva
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);
}

/**
 * @brief Beállítja a gombokat, a kapukimenetet, a Timer1-et és az ADC-t.
 *
 */
void sensorInit() {
    pinMode(PIN_BTN_ZERO, INPUT_PULLUP); // a nulla gombot bemeneti felhúzással
    pinMode(PIN_BTN_CAL, INPUT_PULLUP);  // a kalibrációs gombot bemeneti felhúzással
    DDRD |= _BV(GATE_BIT);               // a kapukimenetet kimeneti módba
    PORTD &= ~_BV(GATE_BIT);             // alapértelmezés szerint kikapcsolja a kaput

    TCCR1A = 0;         // Timer1 normál üzemmód beállítása
    TCCR1B = _BV(CS11); // Timer1 előosztó beállítása 8-ra (1 MHz-es órajel)
    ADMUX = _BV(REFS0); // referenciafeszültség beállítása AVCC-ra

    // letiltja az analóg bemenetek digitális bemenetként való használatát
    DIDR0 |= _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D);

    adcFast();
}

/**
 * @brief Végrehajt egy mérőimpulzust, és a kikapcsolás után két mintát vesz.
 *
 * @param s1 Az első minta kimeneti hivatkozása.
 * @param s2 A referencia-minta kimeneti hivatkozása.
 * @note A mintavétel az ADC-konverzió indulása után körülbelül másfél ADC-órajelre történik.
 *
 * A mintavétel az ADC-konverzió indulása után körülbelül másfél ADC-órajelre
 * történik; a kritikus időzítés alatt a megszakítások le vannak tiltva.
 */
static void pulse(uint16_t &s1, uint16_t &s2) {
    const uint8_t sreg = SREG; // elmenti a megszakítási állapotot
    cli();                     // megszakítások letiltása a kritikus időzítés alatt
    const uint16_t t0 = TCNT1; // a mérőimpulzus kezdőideje
    PORTD |= _BV(GATE_BIT);    // bekapcsolja a kaput

    // várakozik a mérőimpulzus idejére
    while ((uint16_t)(TCNT1 - t0) < T_ON * TICKS_PER_US) {
    }
    PORTD &= ~_BV(GATE_BIT);     // kikapcsolja a kaput
    const uint16_t toff = TCNT1; // a kapu kikapcsolása utáni időpont

    // várakozik az első mintavételi időig
    while ((uint16_t)(TCNT1 - toff) < T_SAMPLE1 * TICKS_PER_US) {
    }
    s1 = adcRead(); // első minta rögzítése

    // várakozik a második mintavételi időig
    while ((uint16_t)(TCNT1 - toff) < T_SAMPLE2 * TICKS_PER_US) {
    }
    s2 = adcRead(); // második minta rögzítése

    SREG = sreg; // visszaállítja a megszakítási állapotot
}

/**
 * @brief Megméri a tekercsek közötti áthallást a Tx bekapcsolt állapotában.
 * @param s1 A bekapcsolás alatti minta kimeneti hivatkozása.
 * @param s2 A nyugalmi referencia-minta kimeneti hivatkozása.
 *
 *
 * A mért feszültség előjeles, és arányos a kölcsönös induktivitással, valamint
 * az áramváltozás sebességével. Az átfedés nullpontján az előjel vált.
 */
static void pulseCoupling(uint16_t &s1, uint16_t &s2) {
    const uint8_t sreg = SREG; // elmenti a megszakítási állapotot
    cli();                     // megszakítások letiltása a kritikus időzítés alatt
    const uint16_t t0 = TCNT1; // a mérőimpulzus kezdőideje
    PORTD |= _BV(GATE_BIT);    // bekapcsolja a kaput

    // várakozik a bekapcsolt állapot idejére
    while ((uint16_t)(TCNT1 - t0) < T_COUPLE * TICKS_PER_US) {
    }
    s1 = adcRead(); // első minta rögzítése a bekapcsolt állapotban

    // várakozik a bekapcsolt állapot idejére a második mintavétel előtt
    while ((uint16_t)(TCNT1 - t0) < T_ON * TICKS_PER_US) {
    }
    PORTD &= ~_BV(GATE_BIT);     // kikapcsolja a kaput
    const uint16_t toff = TCNT1; // a kapu kikapcsolása utáni időpont
    // várakozik a második mintavételi időig
    while ((uint16_t)(TCNT1 - toff) < T_SAMPLE2 * TICKS_PER_US) {
    }
    s2 = adcRead(); // második minta rögzítése

    SREG = sreg; // visszaállítja a megszakítási állapotot
}

/**
 * @brief Rögzíti, ha valamelyik gombot a mérési ciklus alatt megnyomták.
 *
 */
static inline void pollButtons() {

    // Ellenőrzi a a nulla gombhoz tartozó állapotot
    if (!(PIND & _BV(PIN_BTN_ZERO))) {
        g_zeroSeen = true;
    }

    // Ellenőrzi a kalibrációs gombhoz tartozó állapotot
    if (!(PIND & _BV(PIN_BTN_CAL))) {
        g_calSeen = true;
    }
}

/**
 * @brief Összegyűjti a megadott csatorna impulzusonkénti S1 és S2 mintáit.
 *
 * @param sum1 Az S1-minták összegének kimeneti hivatkozása.
 * @param sum2 Az S2-minták összegének kimeneti hivatkozása.
 * @param ch A mintavételezendő ADC-csatorna.
 *
 */
void measure(uint32_t &sum1, uint32_t &sum2, uint8_t ch) {
    sum1 = 0;
    sum2 = 0;
    g_zeroSeen = g_calSeen = false;
    ADMUX = _BV(REFS0) | ch; // beállítja az ADC-csatornát és a referenciafeszültséget
    delayMicroseconds(300);
    (void)adcRead(); // dummy olvasás az ADC stabilizáláshoz

    // Végrehajtja a mérési ciklust N_PULSES impulzusra
    for (uint16_t i = 0; i < N_PULSES; i++) {
        const uint16_t t0 = TCNT1; // a mérési ciklus kezdőideje
        uint16_t a, b;

        // Kiválasztja a megfelelő impulzusgenerálási és mintavételi függvényt a csatorna alapján
        if (ch == 2) {
            pulseCoupling(a, b); // impulzusgenerálás és mintavétel a csatolt állapotban
        } else {
            pulse(a, b); // impulzusgenerálás és mintavétel a nem csatolt állapotban
        }

        sum1 += a;
        sum2 += b;
        pollButtons(); // ellenőrzi a gombok állapotát a mérési ciklus alatt

        // Várakozik a következő mérési ciklus kezdete előtt a teljes periódusig
        while ((uint16_t)(TCNT1 - t0) < T_PERIOD * TICKS_PER_US) {
        }
    }
}

/**
 * @brief Nyolc ADC-mintából kiszámítja az A0 nyugalmi szintjét.
 *
 * @return Az átlagos A0-érték ADC-egységben.
 */
uint16_t readIdleLevel() {

    constexpr uint8_t N_IDLE_SAMPLES = 8; // az A0 nyugalmi szintjének mintaszáma

    uint32_t t = 0;
    for (uint8_t i = 0; i < N_IDLE_SAMPLES; i++) {
        t += adcRead();
    }
    return t / N_IDLE_SAMPLES;
}

/**
 * @brief Megméri az akkumulátor feszültségét a belső referencia használatával.
 *
 * @return A mért akkufeszültség voltban.
 */
float readBattery() {

    adcSlow(); // lassú ADC-sebesség a pontosabb méréshez

    // Átállítja a belső referencia (1.1V) használatára
    ADMUX = _BV(REFS0) | 0x0E;
    delayMicroseconds(1000);

    (void)adcRead(); // dummy olvasás az ADC stabilizáláshoz
    uint32_t s = 0;
    for (uint8_t i = 0; i < 8; i++) { // 8 minta az átlaghoz
        s += adcRead();
    }
    const float vcc = 1.1f * 1023.0f * 8.0f / (float)s; // kiszámítja a Vcc feszültséget a belső referencia alapján
    ADMUX = _BV(REFS0) | 1;
    delayMicroseconds(300);
    (void)adcRead(); // dummy olvasás az ADC stabilizáláshoz

    constexpr uint8_t N_BATTERY_SAMPLES = 8; // az akkufeszültség mintaszáma

    s = 0;
    for (uint8_t i = 0; i < N_BATTERY_SAMPLES; i++) { // 8 mintát veszünk az átlagszámításhoz
        s += adcRead();
    }
    const float vbat = (s / N_BATTERY_SAMPLES) * vcc / 1023.0f * BATT_DIV; // kiszámítja a tényleges akkufeszültséget

    // visszaállítja az ADC-t az alapértelmezett referenciafeszültségre (Vcc)
    ADMUX = _BV(REFS0);
    adcFast(); // visszaállítja az ADC sebességét gyorsra
    delayMicroseconds(300);
    (void)adcRead(); // dummy olvasás az ADC stabilizáláshoz

    return vbat;
}
