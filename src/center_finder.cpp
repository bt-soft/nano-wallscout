#include "center_finder.h"

#include <math.h>

#include "config.h"

void CenterFinder::reset() {
    sample_ = 0;
    startSearch();
}

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

bool CenterFinder::update(float magnitude, float threshold, bool valid, uint32_t now) {
    if (sample_ < UINT16_MAX) {
        ++sample_;
    }
    if ((stage_ == CenterStage::TRACK || stage_ == CenterStage::RETURN_SCAN) &&
        static_cast<uint32_t>(now - stageStartedAt_) >= CENTER_STAGE_TIMEOUT_MS) {
        startSearch();
        return false;
    }
    if (!valid || threshold <= 0) {
        return false;
    }

    const float lowerThreshold = threshold * CENTER_RELEASE_FRACTION;
    const float peakDrop = threshold * CENTER_PEAK_HYSTERESIS_FRACTION;

    switch (stage_) {
    case CenterStage::SEARCH:
        if (magnitude >= threshold) {
            stage_ = CenterStage::CANDIDATE;
            candidateCount_ = 1;
            peakMagnitude_ = magnitude;
            peakSample_ = sample_;
        }
        break;

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

    case CenterStage::TRACK:
        if (magnitude > peakMagnitude_) {
            peakMagnitude_ = magnitude;
            peakSample_ = sample_;
            fallingCount_ = 0;
        } else if (peakMagnitude_ - magnitude >= peakDrop &&
                   previousMagnitude_ - magnitude >= peakDrop * 0.25f) {
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

    case CenterStage::RETURN_SCAN:
        if (!valleySeen_) {
            const float valleyLimit = peakMagnitude_ * (1.0f - CENTER_MIN_RETURN_DROP_FRACTION);
            if (magnitude <= valleyLimit || magnitude < threshold) {
                valleySeen_ = true;
                previousMagnitude_ = magnitude;
            }
            break;
        }

        if (magnitude < lowerThreshold) {
            candidateCount_ = 0;
            fallingCount_ = 0;
            risingCount_ = 0;
            returnMagnitude_ = 0;
            previousMagnitude_ = magnitude;
            break;
        }

        if (risingCount_ < CENTER_CONFIRM_SAMPLES) {
            if (magnitude > previousMagnitude_ + peakDrop * 0.25f) {
                ++risingCount_;
            } else {
                risingCount_ = 0;
            }
            previousMagnitude_ = magnitude;
            break;
        }

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
                } else if (returnMagnitude_ - magnitude >= peakDrop &&
                           previousMagnitude_ - magnitude >= peakDrop * 0.25f) {
                    if (fallingCount_ < UINT8_MAX) {
                        ++fallingCount_;
                    }
                } else {
                    fallingCount_ = 0;
                }

                if (candidateCount_ < UINT8_MAX) {
                    ++candidateCount_;
                }
                if (candidateCount_ >= CENTER_CONFIRM_SAMPLES &&
                    fallingCount_ >= CENTER_FALLING_SAMPLES) {
                    const float allowedDifference =
                        fmaxf(threshold * CENTER_RETURN_MIN_TOLERANCE,
                              peakMagnitude_ * CENTER_RETURN_TOLERANCE_FRACTION);
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
