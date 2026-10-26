#pragma once

#include <Arduino.h>

/**
 * Eltelt már annyi idő?
 */
inline bool timeHasPassed(int32_t fromWhen, int32_t howLong) { return (int32_t)millis() - fromWhen >= howLong; }
