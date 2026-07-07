#pragma once

#include "dsp/algorithms/SweptReverb.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Swept Reverb" algorithm: the SweptReverb
 * Block (see dsp/algorithms/SweptReverb.h) plus input trim.
 */
class SweptReverbAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::SweptReverb::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        reverb_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- SweptReverb Block pass-throughs --
    void setLineDelayMs(int line, float ms) { reverb_.setLineDelayMs(line, ms); }
    void setLineRate(int line, float rate0to100) { reverb_.setLineRate(line, rate0to100); }
    void setLineDepth(int line, float depth0to100) { reverb_.setLineDepth(line, depth0to100); }
    void setMasterDelay(float scale) { reverb_.setMasterDelay(scale); }
    void setMasterRate(float scale) { reverb_.setMasterRate(scale); }
    void setMasterDepth(float scale) { reverb_.setMasterDepth(scale); }
    void setFeedback(float feedback) { reverb_.setFeedback(feedback); }
    void setMix(float wet) { reverb_.setMix(wet); }
    void setRepeat(bool repeat) { reverb_.setRepeat(repeat); }

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
    dsp::algorithms::SweptReverb reverb_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
