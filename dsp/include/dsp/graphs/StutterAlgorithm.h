#pragma once

#include "dsp/algorithms/Stutter.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Stutter" algorithm: the Stutter Block
 * (see dsp/algorithms/Stutter.h) plus independent Left/Right input trim.
 */
class StutterAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Stutter::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- Stutter Block pass-throughs --
    void setLength1(float seconds) { engine_.setLength1(seconds); }
    void setLength2(float seconds) { engine_.setLength2(seconds); }
    void setCount1(int count) { engine_.setCount1(count); }
    void setCount2(int count) { engine_.setCount2(count); }
    void setLeftCoarseFineCents(float cents) { engine_.setLeftCoarseFineCents(cents); }
    void setRightCoarseFineCents(float cents) { engine_.setRightCoarseFineCents(cents); }
    void setLeftDelaySeconds(float seconds) { engine_.setLeftDelaySeconds(seconds); }
    void setRightDelaySeconds(float seconds) { engine_.setRightDelaySeconds(seconds); }
    void setLeftFeedback(float percent) { engine_.setLeftFeedback(percent); }
    void setRightFeedback(float percent) { engine_.setRightFeedback(percent); }
    void setUp1(float rate0to100, float maxCents) { engine_.setUp1(rate0to100, maxCents); }
    void setDn1(float rate0to100, float minCents) { engine_.setDn1(rate0to100, minCents); }
    void setUp2(float rate0to100, float maxCents) { engine_.setUp2(rate0to100, maxCents); }
    void setDn2(float rate0to100, float minCents) { engine_.setDn2(rate0to100, minCents); }
    void setRand1Max(float cents) { engine_.setRand1Max(cents); }
    void setRand2Max(float cents) { engine_.setRand2Max(cents); }
    void setSweepTarget1(dsp::algorithms::Stutter::SweepTarget target) { engine_.setSweepTarget1(target); }
    void setSweepTarget2(dsp::algorithms::Stutter::SweepTarget target) { engine_.setSweepTarget2(target); }
    void setLeftMix(float wet) { engine_.setLeftMix(wet); }
    void setRightMix(float wet) { engine_.setRightMix(wet); }
    void setAuto(bool enabled) { engine_.setAuto(enabled); }
    void setSpeed(float speed0to100) { engine_.setSpeed(speed0to100); }
    void setProgram(dsp::algorithms::Stutter::Program program) { engine_.setProgram(program); }

    void triggerStutter1() { engine_.triggerStutter1(); }
    void triggerStutter2() { engine_.triggerStutter2(); }
    void triggerSweepUp1() { engine_.triggerSweepUp1(); }
    void triggerSweepDown1() { engine_.triggerSweepDown1(); }
    void triggerRandomPitch1() { engine_.triggerRandomPitch1(); }
    void triggerSweepUp2() { engine_.triggerSweepUp2(); }
    void triggerSweepDown2() { engine_.triggerSweepDown2(); }
    void triggerRandomPitch2() { engine_.triggerRandomPitch2(); }

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
    dsp::algorithms::Stutter engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
