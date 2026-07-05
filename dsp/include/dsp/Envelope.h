#pragma once

#include <algorithm>

namespace dsp
{
/**
 * A one-shot linear ramp from a start value to an end value over a fixed
 * duration, then holding at the end value. Building block for anything
 * shaped by a controllable envelope rather than exponential decay: a
 * reverb's initial Attack sharpness, Chamber/Infinite's Shape+Spread
 * build-up/sustain contour, Inverse's Duration+Slope, or muting output
 * briefly while a structural parameter (e.g. Size) changes.
 */
class LinearRamp
{
  public:
    void trigger(float startValue, float endValue, float durationSamples)
    {
        start_ = startValue;
        end_ = endValue;
        durationSamples_ = std::max(durationSamples, 1.0f);
        elapsed_ = 0.0f;
        active_ = true;
    }

    float next()
    {
        if (!active_)
        {
            return end_;
        }
        auto t = std::min(elapsed_ / durationSamples_, 1.0f);
        elapsed_ += 1.0f;
        if (t >= 1.0f)
        {
            active_ = false;
        }
        return start_ + (end_ - start_) * t;
    }

    bool isActive() const { return active_; }

    void reset()
    {
        active_ = false;
        elapsed_ = 0.0f;
    }

  private:
    float start_ = 0.0f;
    float end_ = 0.0f;
    float durationSamples_ = 1.0f;
    float elapsed_ = 0.0f;
    bool active_ = false;
};
}
