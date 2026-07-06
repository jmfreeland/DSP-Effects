#pragma once

#include "dsp/DelayLine.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace dsp
{
/**
 * A delay-line ("granular") real-time pitch shifter, the building block
 * behind every Eventide H3000-style effect (Diatonic Shift, MicroPitch,
 * etc.) - see docs/eventide-h3000-notes.md. The H3000's real hardware
 * trick was re-clocking its D/A to a different rate than the A/D; we
 * can't do that in software running at a fixed sample rate, so this
 * reconstructs the same audible effect the classic way: two read taps
 * into a delay line, their delay swept continuously at a rate set by the
 * target pitch ratio, offset from each other by half a grain so a
 * triangular crossfade masks the delay-wrap discontinuity each tap hits
 * once per grain.
 *
 * y[n] = s1 * w(d1) + s2 * w(d2), where d1/d2 are two delay-line taps
 * sweeping in [0, grainSamples) at rate (1 - ratio) per sample, offset by
 * grainSamples/2, and w is the triangular window peaking at the grain
 * midpoint (0 at both edges). w(d1) + w(d2) is always exactly 1 for this
 * offset, so no separate gain normalization is needed.
 *
 * This trades a small amount of grain-boundary warble (audible as the
 * characteristic "shimmer" of delay-based pitch shifters, as opposed to
 * the cleaner but far costlier phase-vocoder approach) for an
 * implementation that fits the same no-allocation, block-free constraints
 * as the rest of dsp/ - and, not coincidentally, the same constraints the
 * H3000's own fixed-point PELs were built under.
 */
class PitchShifter
{
  public:
    static constexpr float kMinGrainSeconds = 0.01f;
    static constexpr float kMaxGrainSeconds = 0.3f;

    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }

    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;
        updateGrainSamples();
        reset();
    }

    // 10ms..300ms. Shorter grains track transients better but sound more
    // modulated/metallic; longer grains sound smoother but blur transients.
    void setGrainSeconds(float seconds)
    {
        grainSeconds_ = std::clamp(seconds, kMinGrainSeconds, kMaxGrainSeconds);
        updateGrainSamples();
    }

    // >1 shifts up, <1 shifts down, 1 = no shift.
    void setPitchRatio(float ratio) { ratio_ = std::max(ratio, 0.01f); }

    // Convenience: +/- semitones (fractional allowed).
    void setSemitones(float semitones) { setPitchRatio(std::pow(2.0f, semitones / 12.0f)); }

    void reset()
    {
        delay_.reset();
        delay1_ = 0.0f;
        delay2_ = 0.5f * grainSamples_;
    }

    float process(float input)
    {
        delay_.write(input);

        auto step = 1.0f - ratio_;
        delay1_ = wrap(delay1_ + step);
        delay2_ = wrap(delay2_ + step);

        auto w1 = triangularWindow(delay1_);
        auto w2 = triangularWindow(delay2_);

        return delay_.readLinear(delay1_) * w1 + delay_.readLinear(delay2_) * w2;
    }

  private:
    void updateGrainSamples() { grainSamples_ = std::max(grainSeconds_ * sampleRate_, 4.0f); }

    float wrap(float delaySamples) const
    {
        if (delaySamples < 0.0f)
        {
            return delaySamples + grainSamples_;
        }
        if (delaySamples >= grainSamples_)
        {
            return delaySamples - grainSamples_;
        }
        return delaySamples;
    }

    float triangularWindow(float delaySamples) const
    {
        auto x = delaySamples / grainSamples_; // 0..1
        return 1.0f - std::fabs(2.0f * x - 1.0f);
    }

    DelayLine delay_;
    float sampleRate_ = 48000.0f;
    float grainSeconds_ = 0.05f;
    float grainSamples_ = 2400.0f;
    float ratio_ = 1.0f;
    float delay1_ = 0.0f;
    float delay2_ = 1200.0f;
};
}
