#pragma once

#include "dsp/algorithms/Timesqueeze.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Timesqueeze" algorithm: the Timesqueeze
 * Block (see dsp/algorithms/Timesqueeze.h) plus independent Left/Right
 * input trim (matching the manual's own Left In/Right In level
 * parameters). No Mix control - see the Block's own doc comment for why.
 */
class TimesqueezeAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Timesqueeze::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- Timesqueeze Block pass-throughs --
    void setTimePercent(float percent) { engine_.setTimePercent(percent); }
    void setPitchRatio(float ratio) { engine_.setPitchRatio(ratio); }

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
    dsp::algorithms::Timesqueeze engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
