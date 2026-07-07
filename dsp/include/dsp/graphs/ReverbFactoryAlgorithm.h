#pragma once

#include "dsp/algorithms/ReverbFactory.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Reverb Factory" algorithm: the
 * ReverbFactory Block (see dsp/algorithms/ReverbFactory.h) plus input
 * trim.
 */
class ReverbFactoryAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ReverbFactory::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        reverb_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- ReverbFactory Block pass-throughs --
    void setPredelaySeconds(float seconds) { reverb_.setPredelaySeconds(seconds); }
    void setLineDelayMs(int line, float ms) { reverb_.setLineDelayMs(line, ms); }
    void setOnDecaySeconds(float seconds) { reverb_.setOnDecaySeconds(seconds); }
    void setOffDecaySeconds(float seconds) { reverb_.setOffDecaySeconds(seconds); }
    void setGateTimeSeconds(float seconds) { reverb_.setGateTimeSeconds(seconds); }
    void setGateSpeed(float speed0to100) { reverb_.setGateSpeed(speed0to100); }
    void setGateThreshold(float threshold0to1) { reverb_.setGateThreshold(threshold0to1); }
    void setGateEnabled(bool enabled) { reverb_.setGateEnabled(enabled); }
    void setEqCrossoverHz(float hz) { reverb_.setEqCrossoverHz(hz); }
    void setOnEqGainDb(float db) { reverb_.setOnEqGainDb(db); }
    void setOffEqGainDb(float db) { reverb_.setOffEqGainDb(db); }
    void setMix(float wet) { reverb_.setMix(wet); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { reverb_.reset(); }

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

        reverb_.processSample(left, right);
    }

  private:
    dsp::algorithms::ReverbFactory reverb_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
