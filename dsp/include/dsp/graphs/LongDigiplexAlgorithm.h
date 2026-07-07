#pragma once

#include "dsp/algorithms/LongDigiplex.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Long Digiplex" algorithm: the
 * LongDigiplex Block (see dsp/algorithms/LongDigiplex.h) plus input
 * trim on the one channel it reads.
 */
class LongDigiplexAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::LongDigiplex::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        digiplex_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f);
        reset();
    }

    // -- LongDigiplex Block pass-throughs --
    void setDelaySeconds(float seconds) { digiplex_.setDelaySeconds(seconds); }
    void setFeedback(float feedback) { digiplex_.setFeedback(feedback); }
    void setGlide(float response0to100, bool enabled) { digiplex_.setGlide(response0to100, enabled); }
    void setRepeat(bool repeat) { digiplex_.setRepeat(repeat); }
    void setMix(float wet) { digiplex_.setMix(wet); }

    // -- Input conditioning --
    void setInLevel(float level) { inLevel_ = level; }

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
        left *= inLevel_;
        digiplex_.processSample(left, right);
    }

  private:
    dsp::algorithms::LongDigiplex digiplex_;
    float inLevel_ = 1.0f;
};
}
