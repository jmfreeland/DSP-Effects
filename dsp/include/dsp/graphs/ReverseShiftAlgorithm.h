#pragma once

#include "dsp/algorithms/ReverseShift.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Reverse Shift" algorithm: the
 * ReverseShift Block (see dsp/algorithms/ReverseShift.h) plus input
 * trim. No generic stereo-width control, same reasoning as the other
 * H3000 Graphs - the two outputs are independently-splicing Voices, not
 * a stereo pair to rotate.
 */
class ReverseShiftAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ReverseShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        shift_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- ReverseShift Block pass-throughs --
    void setGrainSeconds(float seconds) { shift_.setGrainSeconds(seconds); }
    void setLeftLengthSeconds(float seconds) { shift_.setLeftLengthSeconds(seconds); }
    void setRightLengthSeconds(float seconds) { shift_.setRightLengthSeconds(seconds); }
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
    dsp::algorithms::ReverseShift shift_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
