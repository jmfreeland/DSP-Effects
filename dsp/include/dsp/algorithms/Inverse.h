#pragma once

#include "dsp/Crossover.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/algorithms/ReverbCore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * A Lexicon PCM81-inspired "Inverse": the one reverb core whose decay
 * isn't RT60-exponential at all - the manual describes its envelope
 * slope as controllable through decay, gate, or rise, via **Duration**
 * (time before a hard cutoff) and independent **Low Slope** / **Mid
 * Slope** per band, replacing Low Rt/Mid Rt entirely.
 *
 * setDecaySeconds()/setLowRatio()/setLink() are still inherited from
 * ReverbCore (public API can't un-inherit) but are fixed internally
 * (see prepare()) rather than exposed as Inverse's own controls -
 * Duration + Low Slope + Mid Slope fully replace them conceptually.
 *
 * Why this isn't just "override the recirculating feedback gain" (which
 * is how it was first built, and was wrong): a rising envelope needs to
 * *reveal* material that arrived earlier, but a near-zero recirculating
 * gain during the early part of a rise discards that material from the
 * tank's own memory before the gain ever climbs back up - there's
 * nothing left to reveal. Real gated/reverse reverbs keep the tank
 * itself circulating normally and apply the gate/rise shape as a read-
 * path-only gain on the way out. So Inverse leaves the tank's RT60 decay
 * gains alone (fixed at a generous internal default) and instead
 * overrides ReverbCore's shapeWetOutput() hook: it re-splits the
 * already-computed wet stereo signal into low/high bands with its own
 * Crossover pair, shapes each band by its own envelope, and recombines.
 *
 * Sign convention is the manual's own, confirmed by the primary source
 * (docs/references/lexicon-pcm81-user-guide-rev1.pdf, p.3-6): "Positive
 * slopes create inverse effects, while more even slopes create gated
 * effects. Negative slope values have rather natural reverb tails." So,
 * given normalized position t = elapsedSamples/durationSamples in [0, 1]:
 *
 *   slope < 0 (natural decay tail):  envelope = (1-t)^(1+3*|slope|)   - front-loaded, steeper for larger |slope|
 *   slope == 0 (gate):               envelope = 1                    - flat until the cutoff
 *   slope > 0 (inverse/rise):        envelope = t^(1+3*slope)         - builds toward the cutoff
 *
 * and envelope snaps to 0 once elapsedSamples >= durationSamples (the
 * hard cutoff both gate and rise need, and decay's natural endpoint
 * anyway), retriggered by the same rising-edge transient detector Plate/
 * Chamber use. The curve shape itself (the exponent formula) is an
 * original reconstruction - the manual doesn't specify one - but the
 * sign convention and the decay/gate/rise behavior it produces are
 * verified against the primary source, not guessed. No pre-echo
 * (EkoDly/EkoFbk): the manual scopes that to Plate/Chamber/Infinite only.
 */
class Inverse : public ReverbCore
{
  public:
    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        ReverbCore::prepare(sampleRate, workingBuffer);
        // Fixed, generous internal tank sustain - not user-facing, just
        // enough that material persists in the tank for the Slope
        // envelopes below to shape on the way out (see class comment).
        setDecaySeconds(2.5f);
        setLowRatio(1.0f);
        setCrossoverFrequency(400.0f);

        outputCrossoverLeft_.setFrequency(400.0f, sampleRate);
        outputCrossoverRight_.setFrequency(400.0f, sampleRate);
        transientFollower_.setCoefficient(onePoleLowpassCoefficient(15.0f, sampleRate));
        setDuration(1.0f);
        setLowSlope(-0.3f);
        setMidSlope(-0.3f);
        reset();
    }

    // Time from onset to the hard cutoff, in seconds (0.05..8, matching
    // the same practical range as the other cores' Decay).
    void setDuration(float seconds) { durationSamples_ = std::max(seconds, 0.05f) * sampleRate(); }

    // -1 (natural decay tail: front-loaded, steeper the more negative)
    // .. 0 (gate: flat, then cuts) .. +1 (inverse/rise: builds toward
    // the cutoff) - matches the manual's own sign convention.
    void setLowSlope(float slope) { lowSlope_ = std::clamp(slope, -1.0f, 1.0f); }
    void setMidSlope(float slope) { midSlope_ = std::clamp(slope, -1.0f, 1.0f); }

    void reset()
    {
        ReverbCore::reset();
        outputCrossoverLeft_.reset();
        outputCrossoverRight_.reset();
        transientFollower_.reset();
        elapsedSamples_ = durationSamples_; // start past cutoff: silent until the first onset
        wasTransient_ = false;
    }

  protected:
    void applyPreEcho(float& left, float& right) override
    {
        auto level = 0.5f * (std::fabs(left) + std::fabs(right));
        auto followed = transientFollower_.process(level);
        auto isTransient = level > followed * 1.6f + 0.001f;
        if (isTransient && !wasTransient_)
        {
            elapsedSamples_ = 0.0f;
        }
        wasTransient_ = isTransient;

        if (elapsedSamples_ <= durationSamples_)
        {
            elapsedSamples_ += 1.0f;
        }
    }

    void shapeWetOutput(float& wetLeft, float& wetRight) override
    {
        auto lowEnv = envelopeValue(lowSlope_);
        auto midEnv = envelopeValue(midSlope_);

        auto bandsLeft = outputCrossoverLeft_.process(wetLeft);
        wetLeft = bandsLeft.low * lowEnv + bandsLeft.high * midEnv;

        auto bandsRight = outputCrossoverRight_.process(wetRight);
        wetRight = bandsRight.low * lowEnv + bandsRight.high * midEnv;
    }

  private:
    float envelopeValue(float slope) const
    {
        if (elapsedSamples_ >= durationSamples_)
        {
            return 0.0f;
        }
        auto t = clamp01(elapsedSamples_ / durationSamples_);
        if (slope < 0.0f)
        {
            return std::pow(1.0f - t, 1.0f + 3.0f * -slope);
        }
        if (slope > 0.0f)
        {
            return std::pow(t, 1.0f + 3.0f * slope);
        }
        return 1.0f;
    }

    Crossover outputCrossoverLeft_;
    Crossover outputCrossoverRight_;
    OnePoleLowpass transientFollower_;
    bool wasTransient_ = false;
    float elapsedSamples_ = 0.0f;
    float durationSamples_ = 48000.0f;
    float lowSlope_ = 0.3f;
    float midSlope_ = 0.3f;
};
}
