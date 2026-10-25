#include "sensor.h"

#include <avr/io.h>

#include "config.h"

bool g_zeroSeen = false, g_calSeen = false;

// ---- ADC ----
static inline uint16_t adcRead() {
  ADCSRA |= _BV(ADSC);
  while (ADCSRA & _BV(ADSC)) {
  }
  return ADC;
}

static void adcFast() { ADCSRA = _BV(ADEN) | _BV(ADPS2); }  // /16 = 1 MHz
static void adcSlow() { ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); }  // /128

void sensorInit() {
  pinMode(PIN_BTN_ZERO, INPUT_PULLUP);
  pinMode(PIN_BTN_CAL, INPUT_PULLUP);
  DDRD |= _BV(GATE_BIT);
  PORTD &= ~_BV(GATE_BIT);

  TCCR1A = 0;
  TCCR1B = _BV(CS11);  // normal mod, /8
  ADMUX = _BV(REFS0);
  DIDR0 |= _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D);
  adcFast();
}

// ---- Egy impulzus (megszakitas-tiltott, idokritikus) ----

// A mintavetel a konverzio indulasa utan ~1,5 ADC orajelnel tortenik.
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

// DD beallitas: az Rx nyers feszultsege a Tx bekapcsolt allapotaban (M * dI/dt,
// elojeles), es nyugalmi alapvonal. Az atfedes nullpontjan a kulonbseg elojelet valt.
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

static inline void pollButtons() {
  if (!(PIND & _BV(PIN_BTN_ZERO))) g_zeroSeen = true;
  if (!(PIND & _BV(PIN_BTN_CAL))) g_calSeen = true;
}

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

// ---- Ellenorzesek ----
uint16_t readIdleLevel() {
  uint32_t t = 0;
  for (uint8_t i = 0; i < 8; i++) t += adcRead();
  return t / 8;
}

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
