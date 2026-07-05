#pragma once

#include "dsp/Envelope.h"

#include <algorithm>
#include <cmath>

namespace dsp
{
/**
 * A scalar (in samples) that glides smoothly toward a new target when the
 * change is within a settable range, or jumps instantly otherwise - the
 * PCM81's GldResp/GldRange behavior for Voice and Post-Delay times.
 * Repeated calls to setTarget() with the same value are no-ops, so it's
 * safe to call every block with the current parameter value (as a JUCE
 * processBlock typically does) without re-triggering the glide.
 */
class GlideParameter
{
  public:
    void setSampleRate(float sampleRate) { sampleRate_ = sampleRate; }

    // 0..100; 0 glides over ~60s (imperceptible pitch shift), 100 glides
    // in ~5ms (fast pitch-bend/chirp character).
    void setResponse(float response0to100) { response_ = std::clamp(response0to100, 0.0f, 100.0f); }

    // Maximum |delta| (in seconds) that still glides; anything larger
    // jumps instantly. 0 (the default) means every change jumps instantly.
    void setRangeSeconds(float rangeSeconds) { rangeSamples_ = rangeSeconds * sampleRate_; }

    void setTarget(float samples)
    {
        auto delta = std::fabs(samples - target_);
        if (delta > 0.0001f)
        {
            if (delta > rangeSamples_)
            {
                current_ = samples;
                ramp_.reset();
            }
            else
            {
                auto respNormalized = response_ / 100.0f;
                auto durationSeconds = 60.0f * std::pow(0.005f / 60.0f, respNormalized);
                ramp_.trigger(current_, samples, durationSeconds * sampleRate_);
            }
            target_ = samples;
        }
    }

    float next()
    {
        if (ramp_.isActive())
        {
            current_ = ramp_.next();
        }
        return current_;
    }

    void reset()
    {
        ramp_.reset();
        current_ = target_;
    }

  private:
    float sampleRate_ = 48000.0f;
    float response_ = 50.0f;
    float rangeSamples_ = 0.0f;
    float target_ = 0.0f;
    float current_ = 0.0f;
    LinearRamp ramp_;
};
}
