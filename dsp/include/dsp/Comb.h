#pragma once

#include "dsp/DelayLine.h"

namespace dsp
{
/**
 * Feedback comb filter / recirculating echo: a delay line with a
 * feedback path around it, no direct/inverted mixing (unlike Allpass).
 * The delay length is continuously settable (fractionally interpolated)
 * independent of the buffer capacity, which just bounds the maximum
 * delay. Used both for a reverb's discrete pre-echo (Lexicon's Eko Dly /
 * Eko Fbk) and as the core of a delay Voice (Level/Delay/Feedback/Pan).
 *
 *   y[n] = x[n] + fb * y[n-D]
 */
class Comb
{
  public:
    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }
    void setFeedback(float feedback) { feedback_ = feedback; }

    // Must stay within the buffer's capacity minus one (for interpolation).
    void setDelaySamples(float delaySamples) { delaySamples_ = delaySamples; }

    float process(float input)
    {
        auto delayed = delay_.readLinear(delaySamples_);
        delay_.write(input + feedback_ * delayed);
        return delayed;
    }

    void reset() { delay_.reset(); }

  private:
    DelayLine delay_;
    float feedback_ = 0.0f;
    float delaySamples_ = 0.0f;
};
}
