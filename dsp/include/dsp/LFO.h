#pragma once

#include "dsp/Math.h"

#include <cmath>

namespace dsp
{
/**
 * Simple free-running low-frequency oscillator used to wobble delay lengths
 * (chorus-style modulation inside a reverb tank, pitch-shifter crossfade
 * timing, etc). No allocation, trivially cheap per sample.
 */
class LFO
{
  public:
    void setFrequency(float hz, float sampleRate)
    {
        increment_ = hz / sampleRate;
    }

    void setPhase(float phase01) { phase_ = phase01; }

    float nextSine()
    {
        auto value = std::sin(phase_ * kTwoPi);
        advance();
        return value;
    }

    float nextTriangle()
    {
        // Triangle in [-1, 1] from a 0..1 phase ramp.
        auto value = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;
        advance();
        return value;
    }

  private:
    void advance()
    {
        phase_ += increment_;
        if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
        }
    }

    float phase_ = 0.0f;
    float increment_ = 0.0f;
};
}
