#pragma once

#include "dsp/algorithms/PatchFactory.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Patch Factory" algorithm: the
 * PatchFactory Block (see dsp/algorithms/PatchFactory.h) plus input
 * trim. Left-In only, matching the Block's own Patch Sources list (no
 * "Right Input" source exists in the manual's patch matrix) - the same
 * Left-In-only convention already used for Long/Layered/Reverse Shift.
 */
class PatchFactoryAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::PatchFactory::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f);
        reset();
    }

    // -- PatchFactory Block pass-throughs --
    void setPatch(dsp::algorithms::PatchFactory::Destination destination,
                   dsp::algorithms::PatchFactory::Source source)
    {
        engine_.setPatch(destination, source);
    }
    void setCutoff1(float hz) { engine_.setCutoff1(hz); }
    void setCutoff2(float hz) { engine_.setCutoff2(hz); }
    void setQ1(float q0to1) { engine_.setQ1(q0to1); }
    void setQ2(float q0to1) { engine_.setQ2(q0to1); }
    void setDelay1Seconds(float seconds) { engine_.setDelay1Seconds(seconds); }
    void setDelay2Seconds(float seconds) { engine_.setDelay2Seconds(seconds); }
    void setScale1(float percent) { engine_.setScale1(percent); }
    void setScale2(float percent) { engine_.setScale2(percent); }
    void setShiftCents(float cents) { engine_.setShiftCents(cents); }
    void setPitchDelaySeconds(float seconds) { engine_.setPitchDelaySeconds(seconds); }
    void setLeftMix(float wet) { engine_.setLeftMix(wet); }
    void setRightMix(float wet) { engine_.setRightMix(wet); }

    // -- Input conditioning --
    void setInLevel(float level) { inLevel_ = level; }

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
        left *= inLevel_;
        engine_.processSample(left, right);
    }

  private:
    dsp::algorithms::PatchFactory engine_;
    float inLevel_ = 1.0f;
};
}
