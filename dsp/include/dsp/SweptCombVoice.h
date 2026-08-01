#pragma once

#include "dsp/Comb.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"

#include <algorithm>
#include <cstdint>
#include <span>

namespace dsp
{
/**
 * A single "swept comb" - a Comb (settable-length feedback delay) whose
 * delay length wanders continuously around a base value, driven by
 * LFO::nextRandomWalk() rather than a sine/triangle sweep, plus level and
 * pan - the repeated unit behind the Eventide H3000's Swept Combs (see
 * docs/eventide-swept-combs.md) and, sharing the same "6 lines feeding a
 * stereo mixer" shape, Swept Reverb and Reverb Factory (Algorithms
 * 105-107). The manual's own words for why the sweep is random rather
 * than a regular waveform: "Is it a sine, ramp or triangle sweep?
 * Actually, it's not any of those... The algorithm uses random numbers
 * to achieve a more complex and thicker texture" - see LFO.h's
 * nextRandomWalk() doc comment.
 */
class SweptCombVoice
{
  public:
    void setBuffer(std::span<float> buffer) { comb_.setBuffer(buffer); }

    // seed distinguishes this voice's random-walk sweep from every other
    // SweptCombVoice's - see LFO::setSeed()'s own doc comment for why
    // that matters when several voices' outputs get summed together.
    void prepare(float sampleRate, std::uint32_t seed = 0x9e3779b9u)
    {
        sampleRate_ = sampleRate;
        maxDepthSamples_ = kMaxDepthSeconds * sampleRate_;
        lfo_.setSeed(seed);
        updateSweepFrequency();
    }

    void setBaseDelaySamples(float samples) { baseDelaySamples_ = samples; }

    // 0..100: the sweep generator's own rate, mapped to kMinSweepHz..kMaxSweepHz.
    void setSweepRate(float rate0to100)
    {
        sweepRate01_ = clamp01(rate0to100 / 100.0f);
        updateSweepFrequency();
    }

    // 0..100: how far the delay is allowed to wander, as a percentage of
    // kMaxDepthSamples - the manual states the *existence* of this
    // control but not an absolute ms-per-100% figure, so kMaxDepthSamples
    // is an original (if reasonable) reconstruction - see
    // docs/eventide-swept-combs.md.
    void setSweepDepth(float depth0to100) { depthFraction_ = clamp01(depth0to100 / 100.0f); }

    void setFeedback(float feedback) { comb_.setFeedback(feedback); }
    void setLevel(float level) { level_ = level; }

    // pan in [-1 (full left), 0 (center), +1 (full right)].
    void setPan(float pan)
    {
        panLeft_ = (1.0f - pan) * 0.5f;
        panRight_ = (1.0f + pan) * 0.5f;
    }

    struct Output
    {
        float left;
        float right;
    };

    Output process(float input)
    {
        auto modulated = baseDelaySamples_ + depthFraction_ * maxDepthSamples_ * lfo_.nextRandomWalk();
        comb_.setDelaySamples(std::max(modulated, 0.0f));
        auto voiceOut = comb_.process(input) * level_;
        return { voiceOut * panLeft_, voiceOut * panRight_ };
    }

    void reset() { comb_.reset(); }

  private:
    static constexpr float kMinSweepHz = 0.05f;
    static constexpr float kMaxSweepHz = 5.0f;
    static constexpr float kMaxDepthSeconds = 0.03f;

    void updateSweepFrequency()
    {
        lfo_.setFrequency(mapLinear(sweepRate01_, kMinSweepHz, kMaxSweepHz), sampleRate_);
    }

    Comb comb_;
    LFO lfo_;
    float sampleRate_ = 48000.0f;
    float sweepRate01_ = 0.3f;
    float depthFraction_ = 0.0f;
    float baseDelaySamples_ = 0.0f;
    float maxDepthSamples_ = 0.0f;
    float level_ = 1.0f;
    float panLeft_ = 0.5f;
    float panRight_ = 0.5f;
};
}
