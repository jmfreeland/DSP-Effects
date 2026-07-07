#pragma once

#include "dsp/algorithms/SweptCombs.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Swept Combs" algorithm: the SweptCombs
 * Block (see dsp/algorithms/SweptCombs.h) plus input trim. No generic
 * stereo-width control beyond the Block's own Width master (which scales
 * the six lines' own pans, matching the manual's own parameter, rather
 * than rotating the output afterward).
 */
class SweptCombsAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::SweptCombs::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        combs_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- SweptCombs Block pass-throughs --
    void setLineDelayMs(int line, float ms) { combs_.setLineDelayMs(line, ms); }
    void setLineRate(int line, float rate0to100) { combs_.setLineRate(line, rate0to100); }
    void setLineDepth(int line, float depth0to100) { combs_.setLineDepth(line, depth0to100); }
    void setLineFeedback(int line, float feedback) { combs_.setLineFeedback(line, feedback); }
    void setLinePan(int line, float pan) { combs_.setLinePan(line, pan); }
    void setLineLevel(int line, float level) { combs_.setLineLevel(line, level); }
    void setMasterDelay(float scale) { combs_.setMasterDelay(scale); }
    void setMasterRate(float scale) { combs_.setMasterRate(scale); }
    void setMasterDepth(float scale) { combs_.setMasterDepth(scale); }
    void setMasterFeedback(float scale) { combs_.setMasterFeedback(scale); }
    void setWidth(float scale) { combs_.setWidth(scale); }
    void setMix(float wet) { combs_.setMix(wet); }
    void setStereoInput(bool stereo) { combs_.setStereoInput(stereo); }
    void setRepeat(bool repeat) { combs_.setRepeat(repeat); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { combs_.reset(); }

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

        combs_.processSample(left, right);
    }

  private:
    dsp::algorithms::SweptCombs combs_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
