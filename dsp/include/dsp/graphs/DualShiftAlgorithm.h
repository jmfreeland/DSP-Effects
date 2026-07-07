#pragma once

#include "dsp/algorithms/DualShift.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Dual Shift" algorithm: the DualShift Block
 * (see dsp/algorithms/DualShift.h) plus independent Left/Right input
 * trim - both channels are genuinely independent all the way through, so
 * unlike the other H3000 Graphs there isn't even a shared-vs-independent
 * distinction to note for input handling. No generic stereo-width
 * control, same reasoning as DiatonicShiftAlgorithm/LayeredShiftAlgorithm.
 */
class DualShiftAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::DualShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        shift_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- DualShift Block pass-throughs --
    void setGrainSeconds(float seconds) { shift_.setGrainSeconds(seconds); }
    void setLeftDelaySeconds(float seconds) { shift_.setLeftDelaySeconds(seconds); }
    void setRightDelaySeconds(float seconds) { shift_.setRightDelaySeconds(seconds); }
    void setLeftCents(float cents) { shift_.setLeftCents(cents); }
    void setRightCents(float cents) { shift_.setRightCents(cents); }
    void setLeftFeedback(float amount) { shift_.setLeftFeedback(amount); }
    void setRightFeedback(float amount) { shift_.setRightFeedback(amount); }
    void setLeftMix(float wet) { shift_.setLeftMix(wet); }
    void setRightMix(float wet) { shift_.setRightMix(wet); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { shift_.reset(); }

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

        shift_.processSample(left, right);
    }

  private:
    dsp::algorithms::DualShift shift_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
