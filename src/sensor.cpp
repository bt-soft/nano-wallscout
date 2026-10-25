#include "sensor.h"

#include <avr/io.h>

#include "config.h"

bool g_zeroSeen = false, g_calSeen = false;

/**
 * @brief Elindít egy ADC-konverziót, majd megvárja az eredményt.
 * @return A konverzió eredménye ADC-egységben.
 */
static inline uint16_t adcRead() {
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) {
  }
  return ADC;
}

/**
 * @brief Gyors, 1 MHz-es ADC-órajelet állít be az előosztó 16-ra állításával.
 * @return Nincs visszatérési érték.
 */
static void adcFast() { ADCSRA = _BV(ADEN) | _BV(ADPS2); }

/**
 * @brief Lassú ADC-órajelet állít be az előosztó 128-ra állításával.
 * @return Nincs visszatérési érték.
 */
static void adcSlow() { ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); }

/**
 * @brief Beállítja a gombokat, a kapukimenetet, a Timer1-et és az ADC-t.
 * @return Nincs visszatérési érték.
 */
void sensorInit() {
  pinMode(PIN_BTN_ZERO, INPUT_PULLUP);
  pinMode(PIN_BTN_CAL, INPUT_PULLUP);
  DDRD |= _BV(GATE_BIT);
  PORTD &= ~_BV(GATE_BIT);

  TCCR1A = 0;
  TCCR1B = _BV(CS11);
  ADMUX = _BV(REFS0);
  DIDR0 |= _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D);
  adcFast();
}

/**
 * @brief Végrehajt egy mérőimpulzust, és a kikapcsolás után két mintát vesz.
 * @param s1 Az első minta kimeneti hivatkozása.
 * @param s2 A referencia-minta kimeneti hivatkozása.
 * @return Nincs visszatérési érték.
 *
 * A mintavétel az ADC-konverzió indulása után körülbelül másfél ADC-órajelre
 * történik; a kritikus időzítés alatt a megszakítások le vannak tiltva.
 */
static void pulse(uint16_t& s1, uint16_t& s2) {
  const uint8_t sreg = SREG;
  cli();
  const uint16_t t0 = TCNT1;
  PORTD |= _BV(GATE_BIT);
  while ((uint16_t)(TCNT1 - t0) < T_ON * TICKS_PER_US) {
  }
  PORTD &= ~_BV(GATE_BIT);
  const uint16_t toff = TCNT1;
  while ((uint16_t)(TCNT1 - toff) < T_SAMPLE1 * TICKS_PER_US) {
  }
  s1 = adcRead();
  while ((uint16_t)(TCNT1 - toff) < T_SAMPLE2 * TICKS_PER_US) {
  }
  s2 = adcRead();
  SREG = sreg;
}

/**
 * @brief Megméri a tekercsek közötti áthallást a Tx bekapcsolt állapotában.
 * @param s1 A bekapcsolás alatti minta kimeneti hivatkozása.
 * @param s2 A nyugalmi referencia-minta kimeneti hivatkozása.
 * @return Nincs visszatérési érték.
 *
 * A mért feszültség előjeles, és arányos a kölcsönös induktivitással, valamint
 * az áramváltozás sebességével. Az átfedés nullpontján az előjel vált.
 */
static void pulseCoupling(uint16_t& s1, uint16_t& s2) {
  const uint8_t sreg = SREG;
  cli();
  const uint16_t t0 = TCNT1;
  PORTD |= _BV(GATE_BIT);
  while ((uint16_t)(TCNT1 - t0) < T_COUPLE * TICKS_PER_US) {
  }
  s1 = adcRead();
  while ((uint16_t)(TCNT1 - t0) < T_ON * TICKS_PER_US) {
  }
  PORTD &= ~_BV(GATE_BIT);
  const uint16_t toff = TCNT1;
  while ((uint16_t)(TCNT1 - toff) < T_SAMPLE2 * TICKS_PER_US) {
  }
  s2 = adcRead();
  SREG = sreg;
}

/**
 * @brief Rögzíti, ha valamelyik gombot a mérési ciklus alatt megnyomták.
 * @return Nincs visszatérési érték.
 */
static inline void pollButtons() {
  if (!(PIND & _BV(PIN_BTN_ZERO))) g_zeroSeen = true;
  if (!(PIND & _BV(PIN_BTN_CAL))) g_calSeen = true;
}

/**
 * @brief Összegyűjti a megadott csatorna impulzusonkénti S1 és S2 mintáit.
 * @param sum1 Az S1-minták összegének kimeneti hivatkozása.
 * @param sum2 Az S2-minták összegének kimeneti hivatkozása.
 * @param ch A mintavételezendő ADC-csatorna.
 * @return Nincs visszatérési érték.
 */
void measure(uint32_t& sum1, uint32_t& sum2, uint8_t ch) {
  sum1 = 0;
  sum2 = 0;
  g_zeroSeen = g_calSeen = false;
  ADMUX = _BV(REFS0) | ch;
  delayMicroseconds(300);
  (void)adcRead();
  for (uint16_t i = 0; i < N_PULSES; i++) {
    const uint16_t t0 = TCNT1;
    uint16_t a, b;
    if (ch == 2) {
      pulseCoupling(a, b);
    } else {
      pulse(a, b);
    }
    sum1 += a;
    sum2 += b;
    pollButtons();
    while ((uint16_t)(TCNT1 - t0) < T_PERIOD * TICKS_PER_US) {
    }
  }
}

/**
 * @brief Nyolc ADC-mintából kiszámítja az A0 nyugalmi szintjét.
 * @return Az átlagos A0-érték ADC-egységben.
 */
uint16_t readIdleLevel() {
  uint32_t t = 0;
  for (uint8_t i = 0; i < 8; i++) t += adcRead();
  return t / 8;
}

/**
 * @brief Megméri az akkumulátor feszültségét a belső referencia használatával.
 * @return A mért akkufeszültség voltban.
 */
float readBattery() {
  adcSlow();
  ADMUX = _BV(REFS0) | 0x0E;
  delayMicroseconds(1000);
  (void)adcRead();
  uint32_t s = 0;
  for (uint8_t i = 0; i < 8; i++) s += adcRead();
  const float vcc = 1.1f * 1023.0f * 8.0f / (float)s;
  ADMUX = _BV(REFS0) | 1;
  delayMicroseconds(300);
  (void)adcRead();
  s = 0;
  for (uint8_t i = 0; i < 8; i++) s += adcRead();
  const float vbat = (s / 8.0f) * vcc / 1023.0f * BATT_DIV;
  ADMUX = _BV(REFS0);
  adcFast();
  delayMicroseconds(300);
  (void)adcRead();
  return vbat;
}
