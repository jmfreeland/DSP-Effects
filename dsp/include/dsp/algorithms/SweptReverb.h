#pragma once

#include "dsp/DelayLine.h"
#include "dsp/FeedbackMatrix.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Swept Reverb" - Algorithm 106 (see
 * docs/eventide-h3000-notes.md and docs/eventide-swept-reverb.md). Per
 * the manual: "It is a two input, two output modulated reverb algorithm
 * with tight control over parameters like Feedback, Delay, Rate and
 * Depth." Shares Swept Combs' six-independently-swept-delay-line shape
 * (same random-walk sweep generator per line, same Tedium-style per-line
 * Delay/Rate/Depth with Master proportional scaling), but instead of
 * panning each line's raw output into a stereo mixer (Swept Combs), the
 * six lines feed a genuine feedback delay network - a Householder-mixed
 * "Reverb Network", reusing the same `householderMix()` this archive's
 * Lexicon PCM81 reverb cores already use for their own tank (see
 * `dsp::algorithms::ReverbCore`) - so the six lines diffuse into a
 * continuous reverb tail rather than staying six discrete echoes.
 *
 * The manual doesn't specify the real hardware's internal "Reverb
 * Network" mechanism (not public, per this archive's usual "inspired
 * by" framing - see CLAUDE.md), so reusing the already-built,
 * already-verified Householder FDN pattern from the PCM81 side is a
 * deliberate, documented design choice rather than a guess invented from
 * scratch for this Block specifically.
 */
class SweptReverb
{
  public:
    static constexpr int kNumLines = 6;
    // Matches Algorithm 106's own documented per-line Delay parameter
    // range (0-225ms) plus sweep-depth headroom.
    static constexpr float kMaxLineDelaySeconds = 0.26f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kLineCapacitySamples =
      static_cast<std::size_t>(kMaxLineDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize() { return kNumLines * kLineCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            lines_[idx].setBuffer(
              workingBuffer.subspan(idx * kLineCapacitySamples, kLineCapacitySamples));
        }

        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 37, 53, 71, 97, 131, 179 };
        static constexpr std::array<float, kNumLines> kDefaultRates = { 25, 40, 55, 30, 45, 60 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            baseDelayMs_[idx] = kDefaultDelaysMs[idx];
            baseRate_[idx] = kDefaultRates[idx];
            baseDepth_[idx] = 30.0f;
            sweepLfo_[idx].setPhase(static_cast<float>(i) / static_cast<float>(kNumLines));
        }
        setMasterDelay(1.0f);
        setMasterRate(0.5f);
        setMasterDepth(0.5f);
        setFeedback(0.6f);
        setMix(0.5f);
        setRepeat(false);
        updateSweepFrequencies();
        reset();
    }

    // -- Per-line ("Tedium") parameters, index 0..kNumLines-1 --
    void setLineDelayMs(int line, float ms)
    {
        baseDelayMs_[static_cast<std::size_t>(line)] = std::clamp(ms, 0.0f, 225.0f);
    }
    void setLineRate(int line, float rate0to100)
    {
        baseRate_[static_cast<std::size_t>(line)] = rate0to100;
        updateSweepFrequency(line);
    }
    void setLineDepth(int line, float depth0to100)
    {
        baseDepth_[static_cast<std::size_t>(line)] = depth0to100;
    }

    // -- Master ("Quickset") controls --
    void setMasterDelay(float scale0to1) { masterDelay_ = clamp01(scale0to1); }
    void setMasterRate(float scale0to1)
    {
        masterRate_ = clamp01(scale0to1);
        updateSweepFrequencies();
    }
    void setMasterDepth(float scale0to1) { masterDepth_ = clamp01(scale0to1); }

    // -1..1: negative reverses the feedback phase, matching the manual's
    // own Fdback range and behavior for the other Swept-family algorithms.
    void setFeedback(float feedbackMinus1to1) { feedback_ = std::clamp(feedbackMinus1to1, -1.0f, 1.0f); }

    void setMix(float wet) { mix_ = clamp01(wet); }

    // Holds/repeats the currently-recirculating content by muting new
    // input into the network - matches the manual's Repeat control.
    void setRepeat(bool repeat) { repeat_ = repeat; }

    void reset()
    {
        for (auto& line : lines_) line.reset();
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

        auto input = repeat_ ? 0.0f : 0.5f * (dryLeft + dryRight);

        std::array<float, kNumLines> tapped{};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto depthFraction = (baseDepth_[idx] / 100.0f) * masterDepth_;
            auto depthSamples = depthFraction * kMaxDepthSeconds * sampleRate_;
            auto modSamples = depthSamples * sweepLfo_[idx].nextRandomWalk();
            auto baseSamples = baseDelayMs_[idx] * masterDelay_ * 0.001f * sampleRate_;
            auto delaySamples = std::max(baseSamples + modSamples, 1.0f);
            tapped[idx] = lines_[idx].readLinear(delaySamples);
        }

        auto mixed = tapped;
        householderMix(mixed);

        static constexpr std::array<float, kNumLines> kInputSign = { 1, -1, 1, -1, 1, -1 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto lineInput = input * kInputSign[idx] * 0.5f + mixed[idx] * feedback_;
            lines_[idx].write(lineInput);
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (int i = 0; i < kNumLines; ++i)
        {
            auto tap = tapped[static_cast<std::size_t>(i)];
            wetLeft += tap;
            wetRight += (i % 2 == 0) ? -tap : tap;
        }
        wetLeft *= 0.4f;
        wetRight *= 0.4f;

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(dryRight, wetRight, mix_);
    }

  private:
    // Depth's absolute ms-per-100% scale, matching SweptCombVoice's own
    // original (manual-doesn't-specify-exact-ms) choice - see
    // docs/eventide-swept-reverb.md.
    static constexpr float kMaxDepthSeconds = 0.03f;
    static constexpr float kMinSweepHz = 0.05f;
    static constexpr float kMaxSweepHz = 5.0f;

    void updateSweepFrequency(int line)
    {
        auto idx = static_cast<std::size_t>(line);
        auto rate01 = clamp01(baseRate_[idx] * masterRate_ / 100.0f);
        sweepLfo_[idx].setFrequency(mapLinear(rate01, kMinSweepHz, kMaxSweepHz), sampleRate_);
    }
    void updateSweepFrequencies()
    {
        for (int i = 0; i < kNumLines; ++i) updateSweepFrequency(i);
    }

    std::array<DelayLine, kNumLines> lines_;
    std::array<LFO, kNumLines> sweepLfo_;
    std::array<float, kNumLines> baseDelayMs_{};
    std::array<float, kNumLines> baseRate_{};
    std::array<float, kNumLines> baseDepth_{};

    float sampleRate_ = 48000.0f;
    float masterDelay_ = 1.0f;
    float masterRate_ = 0.5f;
    float masterDepth_ = 0.5f;
    float feedback_ = 0.6f;
    float mix_ = 0.5f;
    bool repeat_ = false;
};
}
