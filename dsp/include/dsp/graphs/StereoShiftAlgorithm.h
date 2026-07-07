#pragma once

#include "dsp/algorithms/StereoShift.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Stereo Shift" algorithm: the StereoShift
 * Block (see dsp/algorithms/StereoShift.h) plus independent Left/Right
 * input trim. No generic stereo-width control, same reasoning as the
 * other H3000 Graphs in this archive - a true stereo pair processed
 * identically doesn't benefit from a post-hoc width rotation the way a
 * mono-derived signal might.
 */
class StereoShiftAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::StereoShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        shift_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- StereoShift Block pass-throughs --
    void setGrainSeconds(float seconds) { shift_.setGrainSeconds(seconds); }
    void setDelaySeconds(float seconds) { shift_.setDelaySeconds(seconds); }
    void setCents(float cents) { shift_.setCents(cents); }
    void setFeedback(float amount) { shift_.setFeedback(amount); }
    void setMix(float wet) { shift_.setMix(wet); }

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
    dsp::algorithms::StereoShift shift_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
