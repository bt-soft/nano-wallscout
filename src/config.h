/**
 * @file A betonvas-kereső közös hardveres és mérési beállításai.
 */
#pragma once

#include <Arduino.h>

/**
 * @brief Az Arduino Nano lábkiosztása a KiCad-kapcsolási rajz szerint.
 *
 * D4: kalibrálógomb; D5: nullázó- és DD-beállítógomb; D2: Q1 kapuja.
 * D11: hangjelző; A0: mérőjel; A1: akkumulátorfeszültség-osztó.
 * A4 és A5: OLED I²C-kapcsolat.
 */
static const uint8_t PIN_BTN_ZERO = 5;
static const uint8_t PIN_BTN_CAL = 4;
#define GATE_BIT PD2

/** @brief A Q1 bekapcsolási ideje mikroszekundumban. */
static const uint16_t T_ON = 60;

/** @brief Az impulzusok periódusideje mikroszekundumban. */
static const uint16_t T_PERIOD = 1000;

/** @brief Az első minta időpontja a Q1 kikapcsolása után, mikroszekundumban. */
static const uint16_t T_SAMPLE1 = 60;

/** @brief A referencia-minta időpontja a Q1 kikapcsolása után,
 * mikroszekundumban. */
static const uint16_t T_SAMPLE2 = 600;

/** @brief A DD-csatolás mintavételi időpontja a Q1 bekapcsolása után,
 * mikroszekundumban. */

static const uint16_t T_COUPLE = 40;
/** @brief Az egy méréshez átlagolt impulzusok száma. */
static const uint16_t N_PULSES = 100;

/** @brief A Timer1 időzítőütéseinek száma mikroszekundumonként. */
static const uint16_t TICKS_PER_US = 2;

/** @brief A találat érzékelési küszöbe ADC-egységben. */
static const float DETECT_TH = 1.5f;

/** @brief A kalibrálás nélküli teljes kijelzőskálához tartozó jel. */
static const float FULL_SCALE = 60.0f;

/** @brief A DD-beállítás logaritmikus skálájának teljes értéke. */
static const float SETUP_FULL = 500.0f;

/** @brief A kalibráláshoz használt ismert mélység centiméterben. */
static const float CAL_DEPTH_CM = 5.0f;

/** @brief Az akkumulátor feszültségosztójának aránya. */
static const float BATT_DIV = (100.0f + 33.0f) / 33.0f;

/** @brief Az indulási nullázáshoz szükséges mérési ciklusok száma. */
static const uint16_t ZERO_UPDATES = 15;

/** @brief A hosszú gombnyomás küszöbideje milliszekundumban. */
static const uint32_t LONG_PRESS_MS = 1000;

/**
 * @brief A nullpont lassú követésének súlya küszöb alatti jelnél.
 */
static const float DRIFT_FOLLOW = 0.04f;

/** @brief Ennyi egymás utáni állandó mérés (≈3 s) után indul az automatikus
 * újranullázás. */
static const uint16_t STEADY_CYCLES = 20;
/** @brief Az automatikus újranullázás súlya mérésenként. */
static const float REZERO_FOLLOW = 0.05f;

/** @brief Küszöb alatti idő az előjelzár feloldásához milliszekundumban. */
static const uint16_t POLARITY_RELEASE_MS = 1000;

/** @brief Az exponenciális simítás (jelzés utáni visszamenet) súlya normál
 * üzemmódban. */
static const float SMOOTH_RUN = 0.5f;

/** @brief Az exponenciális simítás súlya DD-beállító módban. */
static const float SMOOTH_SETUP = 0.1f;

/** @brief Ennél nagyobb ugrásnál a simítás új értékről indul. */
static const float SMOOTH_JUMP = 60.0f;

/** @brief A túlvezérlés felső ADC-küszöbe. */
static const float OVERLOAD_HIGH = 950.0f;

/** @brief A túlvezérlés alsó ADC-küszöbe. */
static const float OVERLOAD_LOW = 70.0f;

/** @brief A DC-hiba felső ADC-küszöbe. */
static const uint16_t IDLE_HIGH = 900;

/** @brief A DC-hiba alsó ADC-küszöbe. */
static const uint16_t IDLE_LOW = 120;

/** @brief Bekapcsolja vagy kikapcsolja a hardveres watchdog használatát. */
#define USE_WATCHDOG 1
