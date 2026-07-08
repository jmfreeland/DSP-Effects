#pragma once

#include "dsp/Math.h"

#include <algorithm>
#include <cmath>

namespace dsp
{
/**
 * A low-frequency oscillator with a much larger waveform vocabulary than
 * the simple sine/triangle/random-walk `LFO` used elsewhere in this
 * archive - the "LFO" module in the Eventide H3000's mod factory
 * algorithms (122/123, see docs/eventide-mod-factory-one.md). Thirteen
 * waveforms in three families, matching that module's own list:
 *
 * - Six continuous waveforms (sine/square/sawtooth/triangle/exponential
 *   sawtooth/exponential triangle) that free-run at `setFrequency()`,
 *   optionally frequency-modulated by `process()`'s input sample.
 * - Five "triggered" waveforms that idle at their resting value until
 *   the input crosses `setThresholdDb()` (a rising edge), then sweep
 *   through exactly one cycle of that shape and hold at the end.
 * - Two "toggle" waveforms (linear/exponential ramp) that alternate
 *   sweeping up and down on each successive trigger, holding at
 *   whichever endpoint they reach.
 *
 * The manual's own prose ("the first 8 waveforms... are continuous; the
 * next 5... are audio-triggered; the last 2... are toggle waves") adds up
 * to 15 against a 13-item list - an internal inconsistency in the source
 * manual (this archive has documented others, e.g. Multi-Shift's
 * Feedback parameter unit mismatch). The item *names* are unambiguous
 * (six are plainly continuous shapes, five are literally named
 * "triggered ...", two are literally named "toggle ..."), so this
 * Primitive follows the names over the inconsistent aggregate count.
 * The exact curve used for the "exponential" shapes isn't given
 * numerically anywhere in the manual (only the descriptive name), so a
 * simple, clearly-documented quadratic curve is used - an original,
 * reasonable reconstruction, not a guess passed off as exact.
 */
class MultiWaveLFO
{
  public:
    enum class Waveform
    {
        kSine,
        kSquare,
        kSawtooth,
        kTriangle,
        kExpSawtooth,
        kExpTriangle,
        kTriggeredSine,
        kTriggeredSaw,
        kTriggeredTriangle,
        kTriggeredExpSaw,
        kTriggeredExpTriangle,
        kToggleLinear,
        kToggleExponential,
    };

    void prepare(float sampleRate) { sampleRate_ = sampleRate; }

    // 0..300Hz, the LFO's own base rate (before Mod).
    void setFrequency(float hz) { baseHz_ = std::max(hz, 0.0f); }
    // 0..300Hz: how much the per-sample input can shift frequency in
    // continuous modes (frequency modulation only - see class comment).
    void setModAmountHz(float hz) { modAmountHz_ = std::max(hz, 0.0f); }
    // 0 to -40dB: the level (converted to linear amplitude) an input must
    // cross, rising, to fire a triggered/toggle sweep.
    void setThresholdDb(float db) { thresholdLinear_ = std::pow(10.0f, std::min(db, 0.0f) / 20.0f); }
    void setWaveform(Waveform waveform) { waveform_ = waveform; }

    void reset()
    {
        phase_ = 0.0f;
        sweeping_ = false;
        heldValue_ = restingValue();
        toggleFrom_ = -1.0f;
        toggleTo_ = 1.0f;
        aboveThreshold_ = false;
    }

    // input serves two roles depending on mode: a frequency-modulation
    // source for continuous waveforms, or a trigger/threshold source for
    // triggered and toggle waveforms.
    float process(float input)
    {
        if (isContinuous())
        {
            auto hz = std::max(baseHz_ + modAmountHz_ * input, 0.0f);
            advancePhase(hz);
            return continuousValue(phase_);
        }

        auto rectified = std::fabs(input);
        auto crossedUp = rectified >= thresholdLinear_ && !aboveThreshold_;
        aboveThreshold_ = rectified >= thresholdLinear_;
        if (crossedUp)
        {
            onTrigger();
        }
        if (!sweeping_)
        {
            return heldValue_;
        }

        // A one-shot sweep (or toggle leg) takes half the LFO's own
        // period per direction, so a full toggle up+down cycle matches
        // the same period a continuous waveform at this frequency would.
        // Unlike advancePhase() (used for free-running waveforms), this
        // must NOT wrap - the sweep needs to actually reach 1.0 and stop.
        auto sweepHz = 2.0f * std::max(baseHz_, 0.01f);
        phase_ += sweepHz / sampleRate_;
        if (phase_ >= 1.0f)
        {
            phase_ = 1.0f;
            sweeping_ = false;
        }
        heldValue_ = isToggle() ? toggleValue(phase_) : triggeredValue(phase_);
        return heldValue_;
    }

  private:
    bool isContinuous() const { return waveform_ <= Waveform::kExpTriangle; }
    bool isToggle() const { return waveform_ == Waveform::kToggleLinear || waveform_ == Waveform::kToggleExponential; }

    void advancePhase(float hz)
    {
        phase_ += hz / sampleRate_;
        if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
        }
    }

    void onTrigger()
    {
        sweeping_ = true;
        phase_ = 0.0f;
        if (isToggle())
        {
            // Sweep away from wherever we're currently resting.
            toggleFrom_ = heldValue_;
            toggleTo_ = -heldValue_;
        }
    }

    float restingValue() const { return isToggle() ? -1.0f : continuousShape(0.0f, triggeredShapeOf(waveform_)); }

    float continuousValue(float phase) const { return continuousShape(phase, waveform_); }

    static Waveform triggeredShapeOf(Waveform triggered)
    {
        switch (triggered)
        {
            case Waveform::kTriggeredSine:
                return Waveform::kSine;
            case Waveform::kTriggeredSaw:
                return Waveform::kSawtooth;
            case Waveform::kTriggeredTriangle:
                return Waveform::kTriangle;
            case Waveform::kTriggeredExpSaw:
                return Waveform::kExpSawtooth;
            case Waveform::kTriggeredExpTriangle:
                return Waveform::kExpTriangle;
            default:
                return Waveform::kSine;
        }
    }

    float triggeredValue(float phase) const { return continuousShape(phase, triggeredShapeOf(waveform_)); }

    float toggleValue(float phase) const
    {
        auto t = waveform_ == Waveform::kToggleExponential ? phase * phase : phase;
        return lerp(toggleFrom_, toggleTo_, t);
    }

    static float continuousShape(float phase, Waveform shape)
    {
        switch (shape)
        {
            case Waveform::kSine:
                return std::sin(phase * kTwoPi);
            case Waveform::kSquare:
                return phase < 0.5f ? 1.0f : -1.0f;
            case Waveform::kSawtooth:
                return 2.0f * phase - 1.0f;
            case Waveform::kTriangle:
                return 4.0f * std::fabs(phase - 0.5f) - 1.0f;
            case Waveform::kExpSawtooth:
                return 2.0f * (phase * phase) - 1.0f;
            case Waveform::kExpTriangle:
            {
                auto tri = phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f; // 0..1..0
                return 2.0f * (tri * tri) - 1.0f;
            }
            default:
                return std::sin(phase * kTwoPi);
        }
    }

    float sampleRate_ = 48000.0f;
    float baseHz_ = 1.0f;
    float modAmountHz_ = 0.0f;
    float thresholdLinear_ = 1.0f;
    Waveform waveform_ = Waveform::kSine;

    float phase_ = 0.0f;
    bool sweeping_ = false;
    float heldValue_ = 0.0f;
    float toggleFrom_ = -1.0f;
    float toggleTo_ = 1.0f;
    bool aboveThreshold_ = false;
};
}
