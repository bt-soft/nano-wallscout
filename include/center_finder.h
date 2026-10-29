#pragma once

#include <stdint.h>

enum class CenterStage : uint8_t {
    SEARCH,
    CANDIDATE,
    TRACK,
    PEAK,
    RETURN_SCAN,
    CENTER_LOCK
};

class CenterFinder {
  public:
    void reset();
    bool update(float magnitude, float threshold, bool valid, uint32_t now);

    CenterStage stage() const { return stage_; }
    float peakMagnitude() const { return peakMagnitude_; }
    float returnMagnitude() const { return returnMagnitude_; }
    uint16_t peakSample() const { return peakSample_; }
    uint16_t returnSample() const { return returnSample_; }

  private:
    void startSearch();

    CenterStage stage_ = CenterStage::SEARCH;
    uint16_t sample_ = 0;
    uint16_t peakSample_ = 0;
    uint16_t returnSample_ = 0;
    uint32_t stageStartedAt_ = 0;
    float peakMagnitude_ = 0;
    float returnMagnitude_ = 0;
    float previousMagnitude_ = 0;
    uint8_t candidateCount_ = 0;
    uint8_t fallingCount_ = 0;
    uint8_t risingCount_ = 0;
    bool valleySeen_ = false;
};

const char *centerStageLabel(CenterStage stage);
