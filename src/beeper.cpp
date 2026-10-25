#include "beeper.h"

#include <avr/io.h>

/**
 * @brief Letiltja a Timer2 kimenetét, és alacsony szintre állítja a hangjelzőt.
 * @return Nincs visszatérési érték.
 */
void beepOff() {
  TCCR2A = 0;
  TCCR2B = 0;
  PORTB &= ~_BV(PB3);
}

/**
 * @brief Kimenetként konfigurálja a hangjelző D11-es lábát, majd kikapcsolja.
 * @return Nincs visszatérési érték.
 */
void beepInit() {
  DDRB |= _BV(PB3);
  beepOff();
}

/**
 * @brief Beállítja a Timer2-t, hogy a kért hangfrekvencián vezérelje a hangjelzőt.
 * @param freqHz A kívánt frekvencia hertzben; a függvény 250–4000 Hz közé korlátozza.
 * @return Nincs visszatérési érték.
 */
void beepOn(uint16_t freqHz) {
  freqHz = constrain(freqHz, 250, 4000);
  OCR2A = (uint8_t)(62500UL / freqHz - 1);
  TCCR2A = _BV(COM2A0) | _BV(WGM21);
  TCCR2B = _BV(CS22) | _BV(CS20);
}
