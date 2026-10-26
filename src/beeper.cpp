#include <avr/io.h>

#include "beeper.h"

/**
 * @brief Letiltja a Timer2 kimenetét, és alacsony szintre állítja a hangjelzőt.
 *
 */
void beepOff() {
    TCCR2A = 0;         // Letiltja a Timer2 kimenetét.
    TCCR2B = 0;         // Letiltja a Timer2-t.
    PORTB &= ~_BV(PB3); // Alacsony szintre állítja a hangjelzőt.
}

/**
 * @brief Kimenetként konfigurálja a hangjelző D11-es lábát, majd kikapcsolja.
 *
 */
void beepInit() {
    DDRB |= _BV(PB3); // Beállítja a D11-es lábat kimenetként.
    beepOff();        // Kikapcsolja a hangjelzőt az inicializálás után.
}

/**
 * @brief Beállítja a Timer2-t, hogy a kért hangfrekvencián vezérelje a hangjelzőt.
 * @param freqHz A kívánt frekvencia hertzben; a függvény 250–4000 Hz közé korlátozza.
 *
 */
void beepOn(uint16_t freqHz) {
    freqHz = constrain(freqHz, 250, 4000);   // Korlátozza a frekvenciát 250–4000 Hz közé.
    OCR2A = (uint8_t)(62500UL / freqHz - 1); // Kiszámolja az OCR2A értékét a kívánt frekvenciához.
    TCCR2A = _BV(COM2A0) | _BV(WGM21);       // Beállítja a Timer2-t CTC módba és a kimeneti összehasonlítást.
    TCCR2B = _BV(CS22) | _BV(CS20);          // Beállítja a Timer2 előosztóját 1024-re.
}
