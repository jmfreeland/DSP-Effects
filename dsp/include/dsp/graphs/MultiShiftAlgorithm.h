#pragma once

#include "dsp/algorithms/MultiShift.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Multi-Shift" algorithm: the MultiShift
 * Block (see dsp/algorithms/MultiShift.h) plus independent Left/Right
 * input trim.
 */
class MultiShiftAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::MultiShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- MultiShift Block pass-throughs --
    void setLeftCents(float cents) { engine_.setLeftCents(cents); }
    void setRightCents(float cents) { engine_.setRightCents(cents); }
    void setLeftPitchDelaySeconds(float seconds) { engine_.setLeftPitchDelaySeconds(seconds); }
    void setRightPitchDelaySeconds(float seconds) { engine_.setRightPitchDelaySeconds(seconds); }
    void setLeftDelaySeconds(float seconds) { engine_.setLeftDelaySeconds(seconds); }
    void setRightDelaySeconds(float seconds) { engine_.setRightDelaySeconds(seconds); }
    void setMix(float wet) { engine_.setMix(wet); }
    void setFeedbackScale(float percent0to100) { engine_.setFeedbackScale(percent0to100); }
    void setImage(float amountMinus1to1) { engine_.setImage(amountMinus1to1); }
    void setLPitchLevel(float percent) { engine_.setLPitchLevel(percent); }
    void setRPitchLevel(float percent) { engine_.setRPitchLevel(percent); }
    void setLDelayLevel(float percent) { engine_.setLDelayLevel(percent); }
    void setRDelayLevel(float percent) { engine_.setRDelayLevel(percent); }
    void setLPitchPan(float pan) { engine_.setLPitchPan(pan); }
    void setRPitchPan(float pan) { engine_.setRPitchPan(pan); }
    void setLDelayPan(float pan) { engine_.setLDelayPan(pan); }
    void setRDelayPan(float pan) { engine_.setRDelayPan(pan); }
    void setLeftFeedback1(float percent, dsp::algorithms::MultiShift::Source source)
    {
        engine_.setLeftFeedback1(percent, source);
    }
    void setLeftFeedback2(float percent, dsp::algorithms::MultiShift::Source source)
    {
        engine_.setLeftFeedback2(percent, source);
    }
    void setRightFeedback1(float percent, dsp::algorithms::MultiShift::Source source)
    {
        engine_.setRightFeedback1(percent, source);
    }
    void setRightFeedback2(float percent, dsp::algorithms::MultiShift::Source source)
    {
        engine_.setRightFeedback2(percent, source);
    }
    void setLeftDirection(bool reverse) { engine_.setLeftDirection(reverse); }
    void setRightDirection(bool reverse) { engine_.setRightDirection(reverse); }
    void setLeftXfadeSlow(bool slow) { engine_.setLeftXfadeSlow(slow); }
    void setRightXfadeSlow(bool slow) { engine_.setRightXfadeSlow(slow); }
    void setLeftSpliceSeconds(float seconds) { engine_.setLeftSpliceSeconds(seconds); }
    void setRightSpliceSeconds(float seconds) { engine_.setRightSpliceSeconds(seconds); }

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
    dsp::algorithms::MultiShift engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
