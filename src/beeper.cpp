#include "beeper.h"

#include <avr/io.h>

void beepOff() {
  TCCR2A = 0;
  TCCR2B = 0;
  PORTB &= ~_BV(PB3);
}

void beepInit() {
  DDRB |= _BV(PB3);
  beepOff();
}

void beepOn(uint16_t freqHz) {
  freqHz = constrain(freqHz, 250, 4000);
  OCR2A = (uint8_t)(62500UL / freqHz - 1);  // 16 MHz / (2 * 128 * f)
  TCCR2A = _BV(COM2A0) | _BV(WGM21);
  TCCR2B = _BV(CS22) | _BV(CS20);
}
