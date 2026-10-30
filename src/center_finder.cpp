#include <math.h>

#include "center_finder.h"
#include "config.h"

/**
 * @file center_finder.cpp
 * @brief Középkereső implementációja.
 *
 * Az állapotgép a középpont keresését valósítja meg különböző fázisokon keresztül.
 * Ezek a fázisok és a működésük:
 * - SEARCH: A középpont keresése a jel nagysága alapján.
 * - CANDIDATE: Lehetséges középpont jelöltjeinek megerősítése.
 * - TRACK: A jel csúcsának követése.
 * - PEAK: A csúcs elérése és a visszakeresés előkészítése.
 * - RETURN_SCAN: A jel visszakeresése a középpont megtalálásához.
 */

/**
 * @brief Középkereső inicializálása.
 */
void CenterFinder::reset() {
    sample_ = 0;
    startSearch();
}

/**
 * @brief Középkereső keresés indítása.
 *
 * Visszaállítja a középkereső állapotát és előkészíti az új keresést.
 */
void CenterFinder::startSearch() {
    stage_ = CenterStage::SEARCH;
    peakSample_ = 0;
    returnSample_ = 0;
    peakMagnitude_ = 0;
    returnMagnitude_ = 0;
    previousMagnitude_ = 0;
    candidateCount_ = 0;
    fallingCount_ = 0;
    risingCount_ = 0;
    valleySeen_ = false;
    stageStartedAt_ = 0;
}

/**
 * @brief Középkereső frissítése új mérési értékkel.
 *
 * @param magnitude A jel nagysága.
 * @param threshold A középkereső küszöbértéke.
 * @param valid A mérés érvényessége.
 * @param now Az aktuális idő milliszekundumban.
 * @return true, ha a középkereső új középpontot talált, false egyébként.
 */
bool CenterFinder::update(float magnitude, float threshold, bool valid, uint32_t now) {

    // Növeljük a mintaszámlálót, ha még nem érte el a maximális értéket.
    if (sample_ < UINT16_MAX) {
        ++sample_;
    }

    // Időtúllépés ellenőrzése a TRACK és RETURN_SCAN állapotokban.
    if ((stage_ == CenterStage::TRACK || stage_ == CenterStage::RETURN_SCAN) && static_cast<uint32_t>(now - stageStartedAt_) >= CENTER_STAGE_TIMEOUT_MS) {
        startSearch();
        return false;
    }

    // Érvényesség és küszöb ellenőrzése.
    if (!valid || threshold <= 0) {
        return false;
    }

    const float lowerThreshold = threshold * CENTER_RELEASE_FRACTION;
    const float peakDrop = threshold * CENTER_PEAK_HYSTERESIS_FRACTION;

    // Küszöbértékek számítása a középkereső logikához.
    switch (stage_) {
        // Középkereső állapotok kezelése.
        case CenterStage::SEARCH:
            if (magnitude >= threshold) {
                stage_ = CenterStage::CANDIDATE;
                candidateCount_ = 1;
                peakMagnitude_ = magnitude;
                peakSample_ = sample_;
            }
            break;

            // Középkereső CANDIDATE állapot kezelése.
        case CenterStage::CANDIDATE:
            if (magnitude < lowerThreshold) {
                startSearch();
            } else {
                if (magnitude > peakMagnitude_) {
                    peakMagnitude_ = magnitude;
                    peakSample_ = sample_;
                }
                if (candidateCount_ < UINT8_MAX) {
                    ++candidateCount_;
                }
                if (candidateCount_ >= CENTER_CONFIRM_SAMPLES) {
                    stage_ = CenterStage::TRACK;
                    stageStartedAt_ = now;
                    previousMagnitude_ = magnitude;
                    fallingCount_ = 0;
                }
            }
            break;

            // Középkereső TRACK állapot kezelése.
        case CenterStage::TRACK:
            if (magnitude > peakMagnitude_) {
                peakMagnitude_ = magnitude;
                peakSample_ = sample_;
                fallingCount_ = 0;
            } else if (peakMagnitude_ - magnitude >= peakDrop && previousMagnitude_ - magnitude >= peakDrop * 0.25f) {
                if (fallingCount_ < UINT8_MAX) {
                    ++fallingCount_;
                }
                if (fallingCount_ >= CENTER_FALLING_SAMPLES) {
                    stage_ = CenterStage::PEAK;
                }
            } else {
                fallingCount_ = 0;
            }
            previousMagnitude_ = magnitude;
            break;

            // Középkereső PEAK állapot kezelése.
        case CenterStage::PEAK:
            stage_ = CenterStage::RETURN_SCAN;
            stageStartedAt_ = now;
            valleySeen_ = false;
            returnMagnitude_ = 0;
            candidateCount_ = 0;
            fallingCount_ = 0;
            risingCount_ = 0;
            previousMagnitude_ = magnitude;
            break;

        // Középkereső RETURN_SCAN állapot kezelése.
        case CenterStage::RETURN_SCAN:
            if (!valleySeen_) {
                const float valleyLimit = peakMagnitude_ * (1.0f - CENTER_MIN_RETURN_DROP_FRACTION);
                if (magnitude <= valleyLimit || magnitude < threshold) {
                    valleySeen_ = true;
                    previousMagnitude_ = magnitude;
                }
                break;
            }

            // Ha a völgy már látható, folytatjuk a visszakeresést.
            if (magnitude < lowerThreshold) {
                candidateCount_ = 0;
                fallingCount_ = 0;
                risingCount_ = 0;
                returnMagnitude_ = 0;
                previousMagnitude_ = magnitude;
                break;
            }

            // A jel emelkedésének megerősítése a CENTER_CONFIRM_SAMPLES minták alapján.
            if (risingCount_ < CENTER_CONFIRM_SAMPLES) {
                if (magnitude > previousMagnitude_ + peakDrop * 0.25f) {
                    ++risingCount_;
                } else {
                    risingCount_ = 0;
                }
                previousMagnitude_ = magnitude;
                break;
            }

            // A jel elérte a megerősített emelkedést, elkezdjük a középpont jelöltjeinek kezelését.
            if (magnitude >= threshold) {
                if (candidateCount_ == 0) {
                    candidateCount_ = 1;
                    returnMagnitude_ = magnitude;
                    returnSample_ = sample_;
                    fallingCount_ = 0;
                } else {
                    if (magnitude > returnMagnitude_) {
                        returnMagnitude_ = magnitude;
                        returnSample_ = sample_;
                        fallingCount_ = 0;
                    } else if (returnMagnitude_ - magnitude >= peakDrop && previousMagnitude_ - magnitude >= peakDrop * 0.25f) {
                        if (fallingCount_ < UINT8_MAX) {
                            ++fallingCount_;
                        }
                    } else {
                        fallingCount_ = 0;
                    }

                    if (candidateCount_ < UINT8_MAX) {
                        ++candidateCount_;
                    }
                    if (candidateCount_ >= CENTER_CONFIRM_SAMPLES && fallingCount_ >= CENTER_FALLING_SAMPLES) {
                        const float allowedDifference = fmaxf(threshold * CENTER_RETURN_MIN_TOLERANCE, peakMagnitude_ * CENTER_RETURN_TOLERANCE_FRACTION);
                        if (fabsf(returnMagnitude_ - peakMagnitude_) <= allowedDifference) {
                            stage_ = CenterStage::CENTER_LOCK;
                            candidateCount_ = 0;
                            return true;
                        }
                        startSearch();
                        break;
                    }
                }
            }
            previousMagnitude_ = magnitude;
            break;

            // Középpont rögzített állapot kezelése.
        case CenterStage::CENTER_LOCK:
            if (magnitude < lowerThreshold) {
                if (candidateCount_ < CENTER_FALLING_SAMPLES) {
                    ++candidateCount_;
                }
                if (candidateCount_ >= CENTER_FALLING_SAMPLES) {
                    startSearch();
                }
            } else {
                candidateCount_ = 0;
            }
            break;
    }

    return false;
}

/**
 * @brief Visszaadja a középpont állapotának szöveges címkéjét.
 *
 * @param stage A középpont állapota.
 * @return A középpont állapotának szöveges címkéje.
 */
const char *centerStageLabel(CenterStage stage) {
    switch (stage) {
        case CenterStage::SEARCH:
            return "";

        case CenterStage::CANDIDATE:
            return "TALALAT";

        case CenterStage::TRACK:
            return "CSUCS KOVETES";

        case CenterStage::PEAK:
            return "CSUCS?";

        case CenterStage::RETURN_SCAN:
            return "PASZTAZZ VISSZA";

        case CenterStage::CENTER_LOCK:
            return "CSUCS MEGEROSITVE";
    }

    return "KERES";
}
