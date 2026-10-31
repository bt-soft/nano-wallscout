#pragma once

#include <stdint.h>

#include "config.h"

/**
 * @brief Gördülő jelgrafikon rögzített méretű körpufferrel (0..32 közötti amplitúdóértékek).
 */
struct SignalGraph {
    int8_t samples[SIGNAL_TIME_GRAPH_SIZE] = {};
    uint8_t count = 0; // Az érvényes minták száma
    uint8_t next = 0;  // A következő írási hely

    void reset() { count = next = 0; }

    void push(int8_t value) {
        samples[next] = value;
        next = (next + 1) % SIGNAL_TIME_GRAPH_SIZE;
        if (count < SIGNAL_TIME_GRAPH_SIZE) {
            ++count;
        }
    }

    /** @brief A i-edik legrégebbi minta (0 = legrégebbi). */
    int8_t at(uint8_t i) const { return samples[count < SIGNAL_TIME_GRAPH_SIZE ? i : (next + i) % SIGNAL_TIME_GRAPH_SIZE]; }
};
