#pragma once

#include "dsp/Diffuser.h"
#include "dsp/DelayLine.h"
#include "dsp/Math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Ultra-Tap" - Algorithm 108 (see
 * docs/eventide-h3000-notes.md and docs/eventide-ultra-tap.md). Per the
 * manual: "Ultra-Tap is a multi-purpose algorithm that has two separate
 * but connected functions. The mono-in, stereo-out program is a diffusor
 * which generates a dense field of delays and it is a series of twelve
 * digital delays connected to a twelve channel stereo mixer... The
 * diffusor is made of a series of four All Pass Filters." Reuses
 * `DiffuserChain<4>` - already built for the Lexicon PCM81 reverb cores'
 * own Diffusion stage - unchanged, plus a new 12-tap cumulative delay
 * line (each tap's own Tap Delay parameter is the time *since the
 * previous tap*, not from the input, per the manual's own worked
 * example) with independent Level/Pan.
 */
class UltraTap
{
  public:
    static constexpr int kNumTaps = 12;
    static constexpr int kNumAllpassStages = 4;
    static constexpr float kMaxAllpassDelaySeconds = 0.8f;
    static constexpr float kMaxTapLineSeconds = 1.45f; // matches the manual's 1.45s collective cap
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kAllpassCapacitySamples =
      static_cast<std::size_t>(kMaxAllpassDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kTapLineCapacitySamples =
      static_cast<std::size_t>(kMaxTapLineSeconds * kMaxSampleRate);

    // The six Spacing/Weights "Quickset" shapes the manual documents,
    // shared between both parameters (same six choices for each).
    enum class Shape
    {
        kConstant,
        kLinearIncreasing,
        kLinearDecreasing,
        kExponentialIncreasing,
        kExponentialDecreasing,
        kRandom
    };

    // The nine Pans "Quickset" configurations.
    enum class PanShape
    {
        kCenter,
        kLeft,
        kRight,
        kSweepLeftToRight,
        kSweepRightToLeft,
        kSpreadFromCenter,
        kMergeToCenter,
        kAlternating,
        kRandom
    };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return kNumAllpassStages * kAllpassCapacitySamples + kTapLineCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        for (int i = 0; i < kNumAllpassStages; ++i)
        {
            diffuser_.setStageBuffer(static_cast<std::size_t>(i),
                                      workingBuffer.subspan(static_cast<std::size_t>(i) * kAllpassCapacitySamples,
                                                             kAllpassCapacitySamples));
        }
        tapLine_.setBuffer(
          workingBuffer.subspan(kNumAllpassStages * kAllpassCapacitySamples, kTapLineCapacitySamples));

        static constexpr std::array<float, kNumAllpassStages> kDefaultAllpassMs = { 20.0f, 15.0f, 11.0f, 7.0f };
        for (int i = 0; i < kNumAllpassStages; ++i)
        {
            setAllpassDelayMs(i, kDefaultAllpassMs[static_cast<std::size_t>(i)]);
        }
        applySpacingShape(Shape::kLinearIncreasing);
        applyWeightsShape(Shape::kLinearDecreasing);
        applyPansShape(PanShape::kSpreadFromCenter);
        setLength(1.0f);
        setDiffusion(0.5f);
        setWidth(1.0f);
        setFeedback(0.0f);
        setFbTap(12);
        setMix(0.5f);
        setStereoInput(true);
        reset();
    }

    // -- Master ("Quickset" numeric) controls --
    void setLength(float scale0to1) { length_ = clamp01(scale0to1); }
    void setDiffusion(float amount0to1) { diffuser_.setDiffusion(clamp01(amount0to1)); }
    void setWidth(float scaleMinus1to1) { width_ = std::clamp(scaleMinus1to1, -1.0f, 1.0f); }
    void setFeedback(float feedbackMinus1to1) { feedback_ = std::clamp(feedbackMinus1to1, -1.0f, 0.99f); }
    // 1..12 (1-based, matching the manual's own "tap 1 to 12" wording).
    void setFbTap(int tap1to12) { fbTap_ = std::clamp(tap1to12, 1, kNumTaps); }
    void setMix(float wet) { mix_ = clamp01(wet); }
    // true: mono-sum L+R. false: left channel only (matches the other
    // H3000 algorithms' own Stereo/Mono expert switch).
    void setStereoInput(bool stereo) { stereoInput_ = stereo; }

    // -- Per-tap ("Tedium") parameters, index 0..kNumTaps-1 --
    // ms since the *previous* tap (tap 0 is since the input), not from
    // the input directly - matches the manual's own worked example.
    void setTapDelayMs(int tap, float ms) { tapDelayMs_[static_cast<std::size_t>(tap)] = std::max(ms, 0.0f); }
    void setTapLevel(int tap, float level0to1) { tapLevel_[static_cast<std::size_t>(tap)] = clamp01(level0to1); }
    void setTapPan(int tap, float panMinus1to1)
    {
        tapPan_[static_cast<std::size_t>(tap)] = std::clamp(panMinus1to1, -1.0f, 1.0f);
    }

    // -- All-pass ("Tedium") delays, index 0..kNumAllpassStages-1 --
    void setAllpassDelayMs(int stage, float ms)
    {
        auto samples = std::clamp(ms, 0.0f, kMaxAllpassDelaySeconds * 1000.0f) * 0.001f * sampleRate_;
        diffuser_.setStageDelaySamples(static_cast<std::size_t>(stage), samples);
    }

    // One-shot "Quickset" generators: overwrite all 12 Tedium values for
    // the given parameter according to the named shape, matching the
    // manual's own note that Quickset "presets" Tedium rather than
    // persistently overlaying it.
    void applySpacingShape(Shape shape) { applyShape(shape, tapDelayMs_, 1400.0f / kNumTaps, true); }
    void applyWeightsShape(Shape shape) { applyShape(shape, tapLevel_, 1.0f, false); }
    void applyPansShape(PanShape shape)
    {
        switch (shape)
        {
            case PanShape::kCenter:
                tapPan_.fill(0.0f);
                break;
            case PanShape::kLeft:
                tapPan_.fill(-1.0f);
                break;
            case PanShape::kRight:
                tapPan_.fill(1.0f);
                break;
            case PanShape::kSweepLeftToRight:
                for (int i = 0; i < kNumTaps; ++i)
                    tapPan_[static_cast<std::size_t>(i)] = mapLinear(static_cast<float>(i) / (kNumTaps - 1), -1.0f, 1.0f);
                break;
            case PanShape::kSweepRightToLeft:
                for (int i = 0; i < kNumTaps; ++i)
                    tapPan_[static_cast<std::size_t>(i)] = mapLinear(static_cast<float>(i) / (kNumTaps - 1), 1.0f, -1.0f);
                break;
            case PanShape::kSpreadFromCenter:
                for (int i = 0; i < kNumTaps; ++i)
                {
                    auto t = static_cast<float>(i) / (kNumTaps - 1);
                    auto side = (i % 2 == 0) ? 1.0f : -1.0f;
                    tapPan_[static_cast<std::size_t>(i)] = side * t;
                }
                break;
            case PanShape::kMergeToCenter:
                for (int i = 0; i < kNumTaps; ++i)
                {
                    auto t = 1.0f - static_cast<float>(i) / (kNumTaps - 1);
                    auto side = (i % 2 == 0) ? 1.0f : -1.0f;
                    tapPan_[static_cast<std::size_t>(i)] = side * t;
                }
                break;
            case PanShape::kAlternating:
                for (int i = 0; i < kNumTaps; ++i)
                    tapPan_[static_cast<std::size_t>(i)] = (i % 2 == 0) ? -1.0f : 1.0f;
                break;
            case PanShape::kRandom:
                for (int i = 0; i < kNumTaps; ++i)
                    tapPan_[static_cast<std::size_t>(i)] = nextRandomBipolar();
                break;
        }
    }

    void reset()
    {
        diffuser_.reset();
        tapLine_.reset();
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    void processSample(float& left, float& right)
    {
        auto dryLeft = left;
        auto dryRight = right;

        auto input = stereoInput_ ? 0.5f * (dryLeft + dryRight) : dryLeft;

        auto diffused = diffuser_.process(input + feedback_ * lastFbTapValue_);

        tapLine_.write(diffused);

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        float cumulativeMs = 0.0f;
        for (int i = 0; i < kNumTaps; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            cumulativeMs += tapDelayMs_[idx] * length_;
            auto delaySamples = cumulativeMs * 0.001f * sampleRate_;
            auto tapValue = tapLine_.readLinear(delaySamples) * tapLevel_[idx];

            if (i + 1 == fbTap_)
            {
                lastFbTapValue_ = tapValue;
            }

            auto pan = std::clamp(tapPan_[idx] * width_, -1.0f, 1.0f);
            auto panLeft = (1.0f - pan) * 0.5f;
            auto panRight = (1.0f + pan) * 0.5f;
            wetLeft += tapValue * panLeft;
            wetRight += tapValue * panRight;
        }

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(dryRight, wetRight, mix_);
    }

  private:
    void applyShape(Shape shape, std::array<float, kNumTaps>& target, float uniformValue, bool normalizeToTotal)
    {
        std::array<float, kNumTaps> weights{};
        switch (shape)
        {
            case Shape::kConstant:
                weights.fill(1.0f);
                break;
            case Shape::kLinearIncreasing:
                for (int i = 0; i < kNumTaps; ++i) weights[static_cast<std::size_t>(i)] = static_cast<float>(i + 1);
                break;
            case Shape::kLinearDecreasing:
                for (int i = 0; i < kNumTaps; ++i)
                    weights[static_cast<std::size_t>(i)] = static_cast<float>(kNumTaps - i);
                break;
            case Shape::kExponentialIncreasing:
                for (int i = 0; i < kNumTaps; ++i)
                    weights[static_cast<std::size_t>(i)] = std::pow(1.5f, static_cast<float>(i));
                break;
            case Shape::kExponentialDecreasing:
                for (int i = 0; i < kNumTaps; ++i)
                    weights[static_cast<std::size_t>(i)] = std::pow(1.5f, static_cast<float>(kNumTaps - 1 - i));
                break;
            case Shape::kRandom:
                for (auto& w : weights) w = 0.2f + 0.8f * nextRandomUnipolar();
                break;
        }

        if (normalizeToTotal)
        {
            float sum = 0.0f;
            for (auto w : weights) sum += w;
            auto total = uniformValue * kNumTaps;
            for (int i = 0; i < kNumTaps; ++i)
                target[static_cast<std::size_t>(i)] = sum > 0.0f ? weights[static_cast<std::size_t>(i)] / sum * total : 0.0f;
        }
        else
        {
            float maxWeight = 0.0f;
            for (auto w : weights) maxWeight = std::max(maxWeight, w);
            for (int i = 0; i < kNumTaps; ++i)
                target[static_cast<std::size_t>(i)] =
                  maxWeight > 0.0f ? weights[static_cast<std::size_t>(i)] / maxWeight * uniformValue : 0.0f;
        }
    }

    float nextRandomUnipolar() { return 0.5f * (nextRandomBipolar() + 1.0f); }

    float nextRandomBipolar()
    {
        rngState_ ^= rngState_ << 13;
        rngState_ ^= rngState_ >> 17;
        rngState_ ^= rngState_ << 5;
        return (static_cast<float>(rngState_) / 4294967295.0f) * 2.0f - 1.0f;
    }

    DiffuserChain<kNumAllpassStages> diffuser_;
    DelayLine tapLine_;
    std::array<float, kNumTaps> tapDelayMs_{};
    std::array<float, kNumTaps> tapLevel_{};
    std::array<float, kNumTaps> tapPan_{};

    float sampleRate_ = 48000.0f;
    float length_ = 1.0f;
    float width_ = 1.0f;
    float feedback_ = 0.0f;
    int fbTap_ = 12;
    float lastFbTapValue_ = 0.0f;
    float mix_ = 0.5f;
    bool stereoInput_ = true;
    std::uint32_t rngState_ = 0x2545F491u;
};
}
