#pragma once

namespace dsp
{
/**
 * One-pole lowpass, used inside reverb feedback paths to model the
 * progressive high-frequency loss of real reverberant spaces and analog
 * reverb tanks ("damping").
 */
class OnePoleLowpass
{
  public:
    // coefficient in [0, 1); higher values damp more high frequency content.
    void setCoefficient(float coefficient) { a_ = coefficient; }

    float process(float input)
    {
        z_ = input + (z_ - input) * a_;
        return z_;
    }

    void reset() { z_ = 0.0f; }

  private:
    float a_ = 0.0f;
    float z_ = 0.0f;
};
}
