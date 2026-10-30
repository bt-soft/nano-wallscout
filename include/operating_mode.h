#pragma once

#include <stdint.h>

/** @brief A készülék kijelzési és mérési üzemmódjai. */
enum class OperatingMode : uint8_t {
    SEARCH,        // Kereső üzemmód
    DD_SETUP,      // DD-beállító mód
    TEACH,         // Tanító mód
    CENTER_FINDER, // Középkereső mód
    SIGNAL_TIME    // Jel-Idő grafikon mód
};
