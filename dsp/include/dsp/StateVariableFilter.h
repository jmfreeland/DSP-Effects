#pragma once

#include <algorithm>
#include <cmath>

namespace dsp
{
/**
 * Chamberlin state-variable filter: one two-pole structure producing
 * lowpass, highpass, and bandpass outputs simultaneously each sample,
 * with a continuously-settable cutoff and a resonance amount that
 * approaches self-oscillation as Q approaches 1 - the "tuneable filter"
 * block in the Eventide H3000's Patch Factory algorithm (#111), whose
 * own manual page states plainly: "At a Q factor of 1, the filter will
 * oscillate at the center frequency."
 *
 * Chamberlin's design trades a hard stability ceiling (cutoff must stay
 * well under sampleRate/6, enforced here via clamping) for a simplicity
 * matching the rest of dsp/'s no-allocation, single-pass-per-sample
 * style - a topology-preserving SVF would remove that ceiling but isn't
 * needed at the 44.1/48kHz rates this archive targets.
 */
class StateVariableFilter
{
  public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; }

    // 0Hz..~sampleRate/6, matching Patch Factory's own 0-7000Hz range at
    // typical sample rates.
    void setCutoff(float cutoffHz)
    {
        auto maxCutoff = sampleRate_ / 6.0f;
        auto clamped = std::clamp(cutoffHz, 1.0f, maxCutoff);
        f_ = 2.0f * std::sin(kPi * clamped / sampleRate_);
    }

    // 0..1: 0 is maximally damped (flat response), 1 approaches
    // self-oscillation at the cutoff frequency.
    void setQ(float q0to1)
    {
        auto q = std::clamp(q0to1, 0.0f, 1.0f);
        damping_ = std::max(2.0f * (1.0f - q), kMinDamping);
    }

    void reset()
    {
        low_ = 0.0f;
        band_ = 0.0f;
    }

    // Advances the filter state and returns {lowpass, highpass, bandpass}.
    struct Outputs
    {
        float lowpass;
        float highpass;
        float bandpass;
    };

    Outputs process(float input)
    {
        low_ += f_ * band_;
        auto high = input - low_ - damping_ * band_;
        band_ += f_ * high;
        low_ = std::clamp(low_, -kClamp, kClamp);
        band_ = std::clamp(band_, -kClamp, kClamp);
        return { low_, high, band_ };
    }

  private:
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kMinDamping = 0.02f;
    static constexpr float kClamp = 100.0f;

    float sampleRate_ = 48000.0f;
    float f_ = 0.1f;
    float damping_ = 1.0f;
    float low_ = 0.0f;
    float band_ = 0.0f;
};
}
