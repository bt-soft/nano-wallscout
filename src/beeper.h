/**
 * @brief A D11 kimenetre kötött, Timer2-vel vezérelt hangjelző kezelése.
 */
#pragma once

#include <Arduino.h>

/**
 * @brief Inicializálja a hangjelző kimenetét.
 * @return Nincs visszatérési érték.
 */
void beepInit();

/**
 * @brief Kikapcsolja a hangjelzőt.
 * @return Nincs visszatérési érték.
 */
void beepOff();

/**
 * @brief Bekapcsolja a hangjelzőt a megadott frekvencián.
 * @param freqHz A kívánt hangfrekvencia hertzben.
 * @return Nincs visszatérési érték.
 */
void beepOn(uint16_t freqHz);
