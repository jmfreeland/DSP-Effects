#pragma once

#include "dsp/Decay.h"
#include "dsp/DelayLine.h"
#include "dsp/Diffuser.h"
#include "dsp/FeedbackMatrix.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Dense Room" (Algorithm 114), per that
 * algorithm's own manual page: "This algorithm offers a much improved
 * early response characteristic over the original 'Reverb Factory'
 * program. In addition, this algorithm has greater reverb 'density' and
 * better control over source positioning... The parametric EQ of Reverb
 * Factory has been replaced by a simple 'high-cut' control, and the
 * noise gate has been removed to allow for the extra-dense processing."
 * A direct evolution of the same 6-line-tank family as
 * `ReverbFactory.h` (Algorithm 107) - same `householderMix()` tank
 * shape - but its own manual page adds a Diffusion stage (reusing
 * `DiffuserChain<3>` from the PCM81 side, with each stage's delay
 * independently runtime-settable per #15-17's Allpass Delay 1-3 - see
 * setAllpassDelaySamples()), explicit per-line Pan/Level (#21-32,
 * unlike Reverb Factory's fixed alternating panning), and drops Reverb
 * Factory's Gate entirely in favor of a single Rev Time. #18-20's
 * independent per-stage Allpass Gain isn't exposed - all three stages
 * share one Diffusion coefficient via `DiffuserChain::setDiffusion()`,
 * the same single-knob approach already used throughout this archive's
 * other diffusion stages (documented as a known simplification).
 *
 * Mono-in (the manual's own Block Diagram draws only a "Left Input"
 * arrow, unlike every stereo-in algorithm in this family) - matching
 * the same convention already used for Long Digiplex/Layered Shift/etc.
 *
 * Position (#4) and Early Mix (#6) reuse two mechanisms already
 * established and documented as original reconstructions on the PCM81
 * side of this archive, since the manual describes their *effect*
 * ("apparent listener location," "coherent vs diffuse early response")
 * without specifying internals: Position blends between the Diffusion
 * stage's own output (an "early," less-processed signal - matching the
 * manual's own Block Diagram, which draws a separate "Early Mix/Pan"
 * path branching directly off Diffusion, parallel to the Reverberator)
 * and the tank's mixed output, the same early/tank balance mechanism as
 * `ReverbCore::setDepth()`. Early Mix blends each line's raw
 * (pre-Householder-mix) tap against its fully-mixed value, the same
 * premix/postmix mechanism as `ReverbCore::setDefinition()`.
 */
class DenseRoom
{
  public:
    static constexpr int kNumLines = 6;
    static constexpr int kNumDiffusers = 3;
    static constexpr float kMaxPreDelaySeconds = 0.5f;
    static constexpr float kMaxLineDelaySeconds = 0.12f; // matches the manual's 5000-sample (~113ms) cap
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kPreDelayCapacitySamples =
      static_cast<std::size_t>(kMaxPreDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kLineCapacitySamples =
      static_cast<std::size_t>(kMaxLineDelaySeconds * kMaxSampleRate);
    // Matches the manual's own Allpass Delay 1-3 expert range (0-5000
    // samples) - much shorter than the tank lines, since these are
    // genuinely runtime-settable via Allpass's opt-in
    // setDelaySamples() extension (see setAllpassDelaySamples() below),
    // not left at their buffer-capacity-derived fixed default.
    static constexpr std::size_t kDiffuserStageCapacitySamples = 5001;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return kPreDelayCapacitySamples + kNumLines * kLineCapacitySamples +
               kNumDiffusers * kDiffuserStageCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        std::size_t offset = 0;
        preDelay_.setBuffer(workingBuffer.subspan(offset, kPreDelayCapacitySamples));
        offset += kPreDelayCapacitySamples;
        for (int i = 0; i < kNumDiffusers; ++i)
        {
            diffuser_.setStageBuffer(static_cast<std::size_t>(i),
                                      workingBuffer.subspan(offset, kDiffuserStageCapacitySamples));
            offset += kDiffuserStageCapacitySamples;
        }
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            lines_[idx].setBuffer(workingBuffer.subspan(offset, kLineCapacitySamples));
            offset += kLineCapacitySamples;
        }

        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 27, 41, 59, 73, 89, 109 };
        static constexpr std::array<float, kNumLines> kDefaultPans = { -0.8f, 0.8f, -0.4f, 0.4f, -0.15f, 0.15f };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            lineDelayMs_[idx] = kDefaultDelaysMs[idx];
            linePan_[idx] = kDefaultPans[idx];
            lineLevel_[idx] = 1.0f;
        }

        // Short, distinct default allpass delays (well under the
        // manual's 5000-sample expert ceiling) so the Diffusion stage
        // reads as a dense cluster of quick echoes rather than its own
        // slowly-decaying secondary reverb - see setAllpassDelaySamples().
        static constexpr std::array<float, kNumDiffusers> kDefaultAllpassDelays = { 337.0f, 563.0f, 809.0f };
        for (int i = 0; i < kNumDiffusers; ++i)
        {
            diffuser_.setStageDelaySamples(static_cast<std::size_t>(i), kDefaultAllpassDelays[static_cast<std::size_t>(i)]);
        }

        setPredelaySeconds(0.02f);
        setRevTimeSeconds(2.0f);
        setHighCut(0.3f);
        setSize(0.7f);
        setPosition(0.3f);
        setPan(0.0f);
        setEarlyMix(0.3f);
        setDiffusion(0.6f);
        setMix(0.5f);
        reset();
    }

    // -- Core controls (#0-8) --
    void setPredelaySeconds(float seconds)
    {
        predelaySamples_ =
          std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kPreDelayCapacitySamples - 2));
    }
    // 0.1s to a very long ("infinity") decay, per the manual's own range.
    void setRevTimeSeconds(float seconds) { revTimeSeconds_ = std::clamp(seconds, 0.1f, 120.0f); }
    // 0 (bright) .. 1 (dark/"warm").
    void setHighCut(float amount0to1)
    {
        auto hz = mapLinear(1.0f - clamp01(amount0to1), 800.0f, 20000.0f);
        for (auto& d : damping_)
        {
            d.setCoefficient(onePoleLowpassCoefficient(hz, sampleRate_));
        }
    }
    // 0 (small room) .. 1 (large room) - scales all six line lengths together.
    void setSize(float amount0to1) { sizeScale_ = mapLinear(clamp01(amount0to1), 0.4f, 1.0f); }
    // 0 (front: early-diffused signal prominent) .. 1 (rear: tank prominent).
    void setPosition(float amount0to1) { position_ = clamp01(amount0to1); }
    // -1 (full left) .. +1 (full right): moves the whole stereo image.
    void setPan(float panMinus1to1) { masterPan_ = std::clamp(panMinus1to1, -1.0f, 1.0f); }
    // 0 (coherent: raw per-line taps) .. 1 (diffuse: fully Householder-mixed).
    void setEarlyMix(float amount0to1) { earlyMixAmount_ = clamp01(amount0to1); }
    void setDiffusion(float amount0to1) { diffusionAmount_ = clamp01(amount0to1); }
    void setMix(float wet) { mix_ = clamp01(wet); }

    // -- Expert controls (#9-32) --
    // 0 to 5000 samples, matching the manual's own Allpass Delay 1-3
    // range - independently runtime-settable via Allpass's opt-in
    // setDelaySamples() extension (the same one built for Ultra-Tap).
    void setAllpassDelaySamples(int stage, float samples)
    {
        diffuser_.setStageDelaySamples(static_cast<std::size_t>(stage), std::clamp(samples, 0.0f, 5000.0f));
    }
    void setLineDelayMs(int line, float ms)
    {
        lineDelayMs_[static_cast<std::size_t>(line)] = std::clamp(ms, 1.0f, 113.0f);
    }
    void setLinePan(int line, float panMinus1to1)
    {
        linePan_[static_cast<std::size_t>(line)] = std::clamp(panMinus1to1, -1.0f, 1.0f);
    }
    // -100..100 per cent; negative inverts phase.
    void setLineLevel(int line, float percent)
    {
        lineLevel_[static_cast<std::size_t>(line)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f;
    }

    void reset()
    {
        preDelay_.reset();
        diffuser_.reset();
        for (auto& line : lines_)
        {
            line.reset();
        }
        for (auto& d : damping_)
        {
            d.reset();
        }
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    // Mono-in (matches the manual's own Block Diagram, which draws only
    // a Left Input) - `right` is used solely as the output channel.
    void processSample(float& left, float& right)
    {
        auto dry = left;

        preDelay_.write(dry);
        auto predelayed = preDelay_.readLinear(predelaySamples_);

        diffuser_.setDiffusion(diffusionAmount_);
        auto diffused = diffuser_.process(predelayed);

        std::array<float, kNumLines> tapped{};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto delaySamples = lineDelayMs_[idx] * 0.001f * sampleRate_ * sizeScale_;
            tapped[idx] = lines_[idx].readLinear(delaySamples);
        }

        std::array<float, kNumLines> decayed{};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto damped = damping_[idx].process(tapped[idx]);
            auto delaySamples = lineDelayMs_[idx] * 0.001f * sampleRate_ * sizeScale_;
            auto gain = rt60ToGain(delaySamples, sampleRate_, revTimeSeconds_);
            decayed[idx] = damped * gain;
        }

        auto premix = decayed;
        householderMix(decayed);

        static constexpr std::array<float, kNumLines> kInputSign = { 1, -1, 1, -1, 1, -1 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            lines_[idx].write(diffused * kInputSign[idx] * 0.5f + decayed[idx]);
        }

        float tankLeft = 0.0f;
        float tankRight = 0.0f;
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto outputTap = lerp(premix[idx], decayed[idx], earlyMixAmount_) * lineLevel_[idx];
            auto panL = 0.5f * (1.0f - linePan_[idx]);
            auto panR = 0.5f * (1.0f + linePan_[idx]);
            tankLeft += outputTap * panL;
            tankRight += outputTap * panR;
        }
        tankLeft *= 0.7f;
        tankRight *= 0.7f;

        auto earlyGain = 2.0f * (1.0f - position_);
        auto tankGain = 2.0f * position_;
        auto earlyPanL = 0.5f * (1.0f - masterPan_);
        auto earlyPanR = 0.5f * (1.0f + masterPan_);
        float wetLeft = diffused * earlyGain * earlyPanL + tankLeft * tankGain;
        float wetRight = diffused * earlyGain * earlyPanR + tankRight * tankGain;

        left = lerp(dry, wetLeft, mix_);
        right = lerp(dry, wetRight, mix_);
    }

  private:
    float sampleRate_ = 48000.0f;

    DelayLine preDelay_;
    DiffuserChain<kNumDiffusers> diffuser_;
    std::array<DelayLine, kNumLines> lines_;
    std::array<OnePoleLowpass, kNumLines> damping_;
    std::array<float, kNumLines> lineDelayMs_{};
    std::array<float, kNumLines> linePan_{};
    std::array<float, kNumLines> lineLevel_{};

    float predelaySamples_ = 0.0f;
    float revTimeSeconds_ = 2.0f;
    float sizeScale_ = 1.0f;
    float position_ = 0.3f;
    float masterPan_ = 0.0f;
    float earlyMixAmount_ = 0.3f;
    float diffusionAmount_ = 0.6f;
    float mix_ = 0.5f;
};
}
