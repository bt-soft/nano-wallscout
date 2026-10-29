#pragma once
/**
 * @brief A D11 kimenetre kötött, Timer2-vel vezérelt hangjelző kezelése.
 */
#include <Arduino.h>

#include "config.h"

/**
 * @brief Inicializálja a hangjelző kimenetét.
 *
 */
void beepInit();

/**
 * @brief Kikapcsolja a hangjelzőt.
 *
 */
void beepOff();

/**
 * @brief Bekapcsolja a hangjelzőt a megadott frekvencián.
 * @param freqHz A kívánt hangfrekvencia hertzben.
 *
 */
void beepOn(uint16_t freqHz);
