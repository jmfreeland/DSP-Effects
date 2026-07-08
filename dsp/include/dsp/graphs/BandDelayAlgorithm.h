#pragma once

#include "dsp/algorithms/BandDelay.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Band Delay" algorithm: the BandDelay
 * Block (see dsp/algorithms/BandDelay.h) plus independent Left/Right
 * input trim.
 */
class BandDelayAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::BandDelay::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- BandDelay Block pass-throughs --
    void setGlobalDelay(float percent0to100) { engine_.setGlobalDelay(percent0to100); }
    void setGlobalFrequency(float semitones) { engine_.setGlobalFrequency(semitones); }
    void setGlobalQ(float percent0to100) { engine_.setGlobalQ(percent0to100); }
    void setGlobalPan(float amountMinus1to1) { engine_.setGlobalPan(amountMinus1to1); }
    void setFeedbackDelaySeconds(float seconds) { engine_.setFeedbackDelaySeconds(seconds); }
    void setFeedback(float percent) { engine_.setFeedback(percent); }
    void setMix(float wet) { engine_.setMix(wet); }
    void setFilterBaseHz(int band, float hz) { engine_.setFilterBaseHz(band, hz); }
    void setFilterFrequencyCents(int band, float cents) { engine_.setFilterFrequencyCents(band, cents); }
    void setFilterQ(int band, float q0to999) { engine_.setFilterQ(band, q0to999); }
    void setFilterDelayMs(int band, float ms) { engine_.setFilterDelayMs(band, ms); }
    void setFilterLevel(int band, float percent) { engine_.setFilterLevel(band, percent); }
    void setFilterPan(int band, float panMinus1to1) { engine_.setFilterPan(band, panMinus1to1); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { engine_.reset(); }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    void processSample(float& left, float& right)
    {
        left *= inLevelLeft_;
        right *= inLevelRight_;
        engine_.processSample(left, right);
    }

  private:
    dsp::algorithms::BandDelay engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
