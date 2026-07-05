#pragma once

#include "dsp/DelayLine.h"

namespace dsp
{
/**
 * Feedback comb filter / recirculating echo: a delay line with a
 * feedback path around it, no direct/inverted mixing (unlike Allpass).
 * Used for the discrete, repeating pre-echo ahead of a reverb tank
 * (Lexicon's Eko Dly / Eko Fbk).
 *
 *   y[n] = x[n] + fb * y[n-D]
 */
class Comb
{
  public:
    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }
    void setFeedback(float feedback) { feedback_ = feedback; }

    float process(float input)
    {
        auto delayed = delay_.read(delay_.size() - 1);
        delay_.write(input + feedback_ * delayed);
        return delayed;
    }

    void reset() { delay_.reset(); }

  private:
    DelayLine delay_;
    float feedback_ = 0.0f;
};
}
