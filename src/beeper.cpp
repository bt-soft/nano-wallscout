#include <avr/io.h>

#include "beeper.h"

/**
 * @brief Letiltja a Timer2 kimenetét, és alacsony szintre állítja a hangjelzőt.
 *
 */
void beepOff() {
    TCCR2A = 0;                // Letiltja a Timer2 kimenetét.
    TCCR2B = 0;                // Letiltja a Timer2-t.
    PORTD &= ~_BV(PIN_BUZZER); // Alacsony szintre állítja a hangjelzőt.
}

/**
 * @brief Kimenetként konfigurálja a hangjelző D11-es lábát, majd kikapcsolja.
 *
 */
void beepInit() {
    DDRD |= _BV(PIN_BUZZER); // Beállítja a D3-as lábat kimenetként.
    beepOff();               // Kikapcsolja a hangjelzőt az inicializálás után.
}

/**
 * @brief Beállítja a Timer2-t, hogy a kért hangfrekvencián vezérelje a hangjelzőt.
 * @param freqHz A kívánt frekvencia hertzben; a függvény 250–4000 Hz közé korlátozza.
 *
 */
void beepOn(uint16_t freqHz) {
    freqHz = constrain(freqHz, 250, 4000);   // Korlátozza a frekvenciát 250–4000 Hz közé.
    OCR2A = (uint8_t)(62500UL / freqHz - 1); // Kiszámolja az OCR2A értékét a kívánt frekvenciához.
    OCR2B = OCR2A;                           // A D3 (OC2B) ugyanabban a pillanatban kapcsol, mint a CTC-csúcs.
    TCCR2A = _BV(COM2B0) | _BV(WGM21);       // CTC mód, az OC2B (D3) kimenet átbillentése egyezéskor.
    TCCR2B = _BV(CS22) | _BV(CS20);          // Beállítja a Timer2 előosztóját 1024-re.
}
