#pragma once

#include "dsp/algorithms/LayeredShift.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Layered Shift" algorithm: the LayeredShift
 * Block (see dsp/algorithms/LayeredShift.h) plus input trim on the one
 * channel the Block actually reads (see that Block's doc comment - the
 * manual's Description names "the left input" as the sole source, so
 * there is no Right In level here the way DiatonicShiftAlgorithm has
 * both). Same reasoning as DiatonicShiftAlgorithm for omitting a generic
 * stereo-width control: the two outputs are independently-shifted Voices,
 * not a stereo pair to rotate.
 */
class LayeredShiftAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::LayeredShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        shift_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f);
        reset();
    }

    // -- LayeredShift Block pass-throughs --
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
    void setInLevel(float level) { inLevel_ = level; }

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
        left *= inLevel_;
        shift_.processSample(left, right);
    }

  private:
    dsp::algorithms::LayeredShift shift_;
    float inLevel_ = 1.0f;
};
}
