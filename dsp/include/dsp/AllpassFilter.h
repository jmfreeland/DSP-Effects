#pragma once

#include <cmath>

namespace dsp
{
/**
 * A first-order allpass *filter*: unity gain at every frequency, but a
 * continuously-settable corner frequency at which the phase shift
 * crosses -90 degrees (approaching -180 degrees well above it, 0 well
 * below). Cascading several of these and mixing the result back with
 * the dry signal produces the moving comb-filter notches of a phaser -
 * the "AP" stages in the Eventide H3000's Phaser (Algorithm 119).
 *
 * Not to be confused with dsp::Allpass (a fixed-delay, feedback-coefficient
 * Schroeder allpass used for reverb diffusion) - despite the shared name,
 * these are different DSP structures for different purposes: one diffuses
 * a signal in time, this one shifts its phase per frequency with no delay
 * at all. Standard first-order allpass form (see e.g. Julius O. Smith's
 * "Introduction to Digital Filters", the "phaser" allpass everyone
 * converges on): H(z) = (a + z^-1) / (1 + a*z^-1).
 */
class AllpassFilter
{
  public:
    // The frequency at which this stage's phase shift crosses -90 degrees.
    void setCutoff(float cutoffHz, float sampleRate)
    {
        auto t = std::tan(kPi * cutoffHz / sampleRate);
        a_ = (t - 1.0f) / (t + 1.0f);
    }

    float process(float input)
    {
        auto output = a_ * input + x1_ - a_ * y1_;
        x1_ = input;
        y1_ = output;
        return output;
    }

    void reset()
    {
        x1_ = 0.0f;
        y1_ = 0.0f;
    }

  private:
    static constexpr float kPi = 3.14159265358979323846f;

    float a_ = 0.0f;
    float x1_ = 0.0f;
    float y1_ = 0.0f;
};
}
