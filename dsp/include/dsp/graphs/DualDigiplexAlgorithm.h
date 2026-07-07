#pragma once

#include "dsp/algorithms/DualDigiplex.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Dual Digiplex" algorithm: the
 * DualDigiplex Block (see dsp/algorithms/DualDigiplex.h) plus
 * independent Left/Right input trim.
 */
class DualDigiplexAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::DualDigiplex::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        digiplex_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- DualDigiplex Block pass-throughs --
    void setLeftDelaySeconds(float seconds) { digiplex_.setLeftDelaySeconds(seconds); }
    void setRightDelaySeconds(float seconds) { digiplex_.setRightDelaySeconds(seconds); }
    void setLeftFeedback(float feedback) { digiplex_.setLeftFeedback(feedback); }
    void setRightFeedback(float feedback) { digiplex_.setRightFeedback(feedback); }
    void setGlide(float response0to100, bool enabled) { digiplex_.setGlide(response0to100, enabled); }
    void setRepeat(bool repeat) { digiplex_.setRepeat(repeat); }
    void setLeftMix(float wet) { digiplex_.setLeftMix(wet); }
    void setRightMix(float wet) { digiplex_.setRightMix(wet); }
    void setStereoInput(bool stereo) { digiplex_.setStereoInput(stereo); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { digiplex_.reset(); }

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
        digiplex_.processSample(left, right);
    }

  private:
    dsp::algorithms::DualDigiplex digiplex_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
