#pragma once

#include "dsp/DelayLine.h"

namespace dsp
{
/**
 * Schroeder allpass filter: a fixed-length delay wrapped in unity-gain
 * feedback/feedforward. Chains of these are the classic way to diffuse a
 * transient into a dense series of echoes before it enters a reverb tank,
 * without coloring the frequency response.
 *
 *   w[n] = x[n] + g * w[n-D]
 *   y[n] = -g * w[n] + w[n-D]
 */
class Allpass
{
  public:
    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }

    // Feedback/feedforward coefficient, |g| < 1. Typical diffusion values: 0.5-0.7.
    void setCoefficient(float g) { g_ = g; }

    float process(float input)
    {
        auto delayed = delay_.read(delay_.size() - 1);
        auto w = input + g_ * delayed;
        delay_.write(w);
        return delayed - g_ * w;
    }

    void reset() { delay_.reset(); }

  private:
    DelayLine delay_;
    float g_ = 0.5f;
};

/**
 * Allpass variant whose delay length is wobbled by an external modulation
 * source (typically an LFO) via fractional interpolated reads. This breaks
 * up the otherwise-static comb spacing of a diffuser/tank, which is what
 * keeps a dense reverb tail sounding alive rather than metallic - a
 * signature trait of Lexicon-style algorithmic reverbs.
 *
 * The write tap stays at a fixed integer offset so the structure remains
 * stable; only the read tap is modulated, which is the standard practical
 * compromise used in modulated allpass/comb designs (e.g. Dattorro's plate).
 */
class ModulatedAllpass
{
  public:
    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }
    void setCoefficient(float g) { g_ = g; }

    // baseDelay in samples; modDepth in samples (peak deviation).
    void setModulation(float baseDelay, float modDepth)
    {
        baseDelay_ = baseDelay;
        modDepth_ = modDepth;
    }

    // modulation in [-1, 1], typically an LFO output.
    float process(float input, float modulation)
    {
        auto delaySamples = baseDelay_ + modulation * modDepth_;
        auto delayed = delay_.readLinear(delaySamples);
        auto w = input + g_ * delayed;
        delay_.write(w);
        return delayed - g_ * w;
    }

    void reset() { delay_.reset(); }

  private:
    DelayLine delay_;
    float g_ = 0.5f;
    float baseDelay_ = 0.0f;
    float modDepth_ = 0.0f;
};
}
