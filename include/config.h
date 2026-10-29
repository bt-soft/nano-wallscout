/**
 * @file A betonvas-kereső közös hardveres és mérési beállításai.
 */
#pragma once

#include <Arduino.h>

/**
 * @brief Az Arduino Nano lábkiosztása a kapcsolási rajz szerint.
 *
 * D4: kalibrálógomb;
 * D5: nullázó- és DD-beállítógomb;
 * D2: Q1 FET kapuja.
 * D3: hangjelző;
 * A0: mérőjel;
 * A1: akkumulátorfeszültség-osztó.
 * A4 és A5: OLED I²C-kapcsolat.
 */
#define GATE_BIT PD2     // Q1 kapu lábának bitje a PORTD regiszterben
#define PIN_BUZZER PD3   // Hangjelző lábának bitje a PORTD regiszterben
#define PIN_BTN_CAL PD4  // Kalibrálógomb lába
#define PIN_BTN_ZERO PD5 // Nullázó- és DD-beállítógomb lába

/** @brief A Q1 bekapcsolási ideje mikroszekundumban. */
static const uint16_t T_ON = 60;

/** @brief Az impulzusok periódusideje mikroszekundumban. */
static const uint16_t T_PERIOD = 1000;

/** @brief Az első minta időpontja a Q1 kikapcsolása után, mikroszekundumban. */
static const uint16_t T_SAMPLE1 = 60;

/** @brief A referencia-minta időpontja a Q1 kikapcsolása után,
 * mikroszekundumban. */
static const uint16_t T_SAMPLE2 = 600;

/** @brief A DD-csatolás mintavételi időpontja a Q1 bekapcsolása után,  mikroszekundumban. */
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

/** @brief Az akkumulátor feszültségosztójának aránya. (100k + 33k) / 33k */
static const float BATT_DIV = (100.0f + 33.0f) / 33.0f;

/** @brief Az indulási nullázáshoz szükséges mérési ciklusok száma. */
static const uint16_t ZERO_UPDATES = 15;

/** @brief A hosszú gombnyomás küszöbideje milliszekundumban. */
static const uint32_t LONG_PRESS_MS = 1000;

/** @brief A nullpont lassú követésének súlya küszöb alatti jelnél. */
static const float DRIFT_FOLLOW = 0.04f;

/** @brief Ennyi egymás utáni állandó mérés (≈3 s) után indul az automatikus újranullázás. */
static const uint16_t STEADY_CYCLES = 20;

/** @brief Az automatikus újranullázás súlya mérésenként. */
static const float REZERO_FOLLOW = 0.05f;

/** @brief Küszöb alatti idő az előjelzár feloldásához milliszekundumban. */
static const uint16_t POLARITY_RELEASE_MS = 1000;

/** @brief Ennyi egymást követő küszöb feletti mérés kell a jelölt megerősítéséhez. */
static const uint8_t CENTER_CONFIRM_SAMPLES = 2;

/** @brief Ennyi csökkenő mérés kell egy lehetséges csúcshoz. */
static const uint8_t CENTER_FALLING_SAMPLES = 3;

/** @brief Időtúllépés a csúcskövetési és a visszapásztázási szakaszra milliszekundumban. */
static const uint32_t CENTER_STAGE_TIMEOUT_MS = 20000;

/** @brief A keresési küszöb alatti visszaesés aránya a jelölt törléséhez. */
static const float CENTER_RELEASE_FRACTION = 0.70f;

/** @brief A csúcstól elvárt minimális visszaesés aránya a zajküszöbhöz képest. */
static const float CENTER_PEAK_HYSTERESIS_FRACTION = 0.20f;

/** @brief A visszapásztázás előtt szükséges minimális jelcsökkenés az első csúcshoz képest. */
static const float CENTER_MIN_RETURN_DROP_FRACTION = 0.30f;

/** @brief A két pásztázás csúcserőssége közötti megengedett relatív eltérés. */
static const float CENTER_RETURN_TOLERANCE_FRACTION = 0.35f;

/** @brief A visszatérő csúcs minimális abszolút eltéréstűrése a becsült zajküszöb többszörösében. */
static const float CENTER_RETURN_MIN_TOLERANCE = 2.0f;

/** @brief E feletti adaptív küszöbnél a középkereső zajos mérést jelez. Hangolása valós mérést igényel. */
static const float CENTER_NOISY_THRESHOLD = 8.0f;

/** @brief A középkereső gördülő előjeles jelgrafikonjának fix elemszáma. */
static const uint8_t CENTER_HISTORY_SIZE = 32;

/** @brief Az exponenciális simítás (jelzés utáni visszamenet) súlya normál üzemmódban. A kisebb érték lassabban követi a jelváltozásokat.  */
static const float SMOOTH_RUN = 0.5f;

/** @brief Az exponenciális simítás súlya DD-beállító módban. A kisebb érték lassabban követi a jelváltozásokat. */
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

/** @brief Bekapcsolja vagy kikapcsolja a hardveres watchdog használatát.
 * A watchdog a rendszer lefagyása esetén újraindítja a kütyüt
 */
#define USE_WATCHDOG 1
