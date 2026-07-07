#pragma once

#include "dsp/algorithms/UltraTap.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Ultra-Tap" algorithm: the UltraTap Block
 * (see dsp/algorithms/UltraTap.h) plus input trim.
 */
class UltraTapAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::UltraTap::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        tap_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- UltraTap Block pass-throughs --
    void setLength(float scale) { tap_.setLength(scale); }
    void setDiffusion(float amount) { tap_.setDiffusion(amount); }
    void setWidth(float scale) { tap_.setWidth(scale); }
    void setFeedback(float feedback) { tap_.setFeedback(feedback); }
    void setFbTap(int tap1to12) { tap_.setFbTap(tap1to12); }
    void setMix(float wet) { tap_.setMix(wet); }
    void setStereoInput(bool stereo) { tap_.setStereoInput(stereo); }
    void setTapDelayMs(int tap, float ms) { tap_.setTapDelayMs(tap, ms); }
    void setTapLevel(int tap, float level) { tap_.setTapLevel(tap, level); }
    void setTapPan(int tap, float pan) { tap_.setTapPan(tap, pan); }
    void setAllpassDelayMs(int stage, float ms) { tap_.setAllpassDelayMs(stage, ms); }
    void applySpacingShape(dsp::algorithms::UltraTap::Shape shape) { tap_.applySpacingShape(shape); }
    void applyWeightsShape(dsp::algorithms::UltraTap::Shape shape) { tap_.applyWeightsShape(shape); }
    void applyPansShape(dsp::algorithms::UltraTap::PanShape shape) { tap_.applyPansShape(shape); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { tap_.reset(); }

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

        tap_.processSample(left, right);
    }

  private:
    dsp::algorithms::UltraTap tap_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
