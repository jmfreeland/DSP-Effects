#pragma once

#include "dsp/algorithms/Vocoder.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Vocoder" algorithm: the Vocoder Block
 * (see dsp/algorithms/Vocoder.h) plus independent Left/Right input trim
 * (matching the manual's own Left In/Right In level parameters - Left
 * is the synthesis/instrument input, Right is the analysis/voice
 * input).
 */
class VocoderAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Vocoder::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- Vocoder Block pass-throughs --
    void setFormantSpeed(float speed0to100) { engine_.setFormantSpeed(speed0to100); }
    void setEnvelopeSpeed(float speed0to100) { engine_.setEnvelopeSpeed(speed0to100); }
    void setFormantShift(float amount0to100) { engine_.setFormantShift(amount0to100); }
    void setDepth(float amount0to1) { engine_.setDepth(amount0to1); }
    void setWidthSeconds(float seconds) { engine_.setWidthSeconds(seconds); }
    void setMix(float wet) { engine_.setMix(wet); }
    void setMaxResonance(float amount0to100) { engine_.setMaxResonance(amount0to100); }
    void setThreshold(float level0to1) { engine_.setThreshold(level0to1); }

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
    dsp::algorithms::Vocoder engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
