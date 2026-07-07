#pragma once

#include "dsp/Math.h"
#include "dsp/SweptCombVoice.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Swept Combs" - Algorithm 105 (see
 * docs/eventide-h3000-notes.md and docs/eventide-swept-combs.md). Per
 * the manual: "Picture six high quality digital delay units racked
 * together; each has 1/4 second delay, modulation control and feedback;
 * all are patched to a 6 input, stereo mixer. Automation allows
 * simultaneous control over the digital delays and mixer or separate
 * control over each."
 *
 * Six independent dsp::SweptCombVoice lines, each with its own base
 * Delay/Rate/Depth/Feedback/Pan/Level (the manual's "Tedium" per-line
 * parameters), summed into a stereo mix. Five master controls (m Delay,
 * m Rate, m Depth, m Fdback, Width) scale all six lines' values
 * proportionally at once - matching the manual's own description of
 * "automation... simultaneous... or separate" - without altering the
 * underlying per-line ("Tedium") values, so returning a master to 100%
 * restores exactly what was set per line.
 */
class SweptCombs
{
  public:
    static constexpr int kNumLines = 6;
    static constexpr float kMaxLineDelaySeconds = 0.3f; // 1/4s + sweep depth headroom
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kLineCapacitySamples =
      static_cast<std::size_t>(kMaxLineDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize() { return kNumLines * kLineCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        for (int i = 0; i < kNumLines; ++i)
        {
            lines_[static_cast<std::size_t>(i)].setBuffer(
              workingBuffer.subspan(static_cast<std::size_t>(i) * kLineCapacitySamples,
                                     kLineCapacitySamples));
            lines_[static_cast<std::size_t>(i)].prepare(sampleRate);
        }

        // Reasonable spread across the 0-250ms range, not all identical,
        // so the algorithm is immediately usable before any Tedium
        // editing - an original default choice (the manual documents the
        // parameters, not factory default values for this algorithm).
        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 41, 67, 93, 127, 163, 211 };
        static constexpr std::array<float, kNumLines> kDefaultRates = { 20, 35, 50, 65, 80, 95 };
        static constexpr std::array<float, kNumLines> kDefaultPans = { -10, -6, -2, 2, 6, 10 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            setLineDelayMs(i, kDefaultDelaysMs[idx]);
            setLineRate(i, kDefaultRates[idx]);
            setLineDepth(i, 30.0f);
            setLineFeedback(i, 0.2f);
            setLinePan(i, kDefaultPans[idx] / 10.0f);
            setLineLevel(i, 0.8f);
        }
        setMasterDelay(1.0f);
        setMasterRate(0.5f);
        setMasterDepth(0.5f);
        setMasterFeedback(1.0f);
        setWidth(1.0f);
        setMix(0.5f);
        setStereoInput(true);
        setRepeat(false);
        reset();
    }

    // -- Per-line ("Tedium") parameters, index 0..kNumLines-1 --
    void setLineDelayMs(int line, float ms)
    {
        baseDelayMs_[static_cast<std::size_t>(line)] = std::clamp(ms, 0.0f, 250.0f);
        applyLineDelay(line);
    }
    void setLineRate(int line, float rate0to100)
    {
        baseRate_[static_cast<std::size_t>(line)] = rate0to100;
        applyLineRate(line);
    }
    void setLineDepth(int line, float depth0to100)
    {
        baseDepth_[static_cast<std::size_t>(line)] = depth0to100;
        applyLineDepth(line);
    }
    void setLineFeedback(int line, float feedbackMinus1to1)
    {
        baseFeedback_[static_cast<std::size_t>(line)] = std::clamp(feedbackMinus1to1, -1.0f, 1.0f);
        applyLineFeedback(line);
    }
    void setLinePan(int line, float panMinus1to1)
    {
        basePan_[static_cast<std::size_t>(line)] = std::clamp(panMinus1to1, -1.0f, 1.0f);
        applyLinePan(line);
    }
    void setLineLevel(int line, float level0to1)
    {
        lines_[static_cast<std::size_t>(line)].setLevel(clamp01(level0to1));
    }

    // -- Master ("Quickset") controls, proportional scalers over the
    // per-line values above --
    void setMasterDelay(float scale0to1)
    {
        masterDelay_ = clamp01(scale0to1);
        for (int i = 0; i < kNumLines; ++i) applyLineDelay(i);
    }
    void setMasterRate(float scale0to1)
    {
        masterRate_ = clamp01(scale0to1);
        for (int i = 0; i < kNumLines; ++i) applyLineRate(i);
    }
    void setMasterDepth(float scale0to1)
    {
        masterDepth_ = clamp01(scale0to1);
        for (int i = 0; i < kNumLines; ++i) applyLineDepth(i);
    }
    // -1..1: negative reverses feedback phase, matching the manual.
    void setMasterFeedback(float scaleMinus1to1)
    {
        masterFeedback_ = std::clamp(scaleMinus1to1, -1.0f, 1.0f);
        for (int i = 0; i < kNumLines; ++i) applyLineFeedback(i);
    }
    // -1..1: negative reverses the stereo image, matching the manual.
    void setWidth(float scaleMinus1to1)
    {
        width_ = std::clamp(scaleMinus1to1, -1.0f, 1.0f);
        for (int i = 0; i < kNumLines; ++i) applyLinePan(i);
    }

    void setMix(float wet) { mix_ = clamp01(wet); }

    // true: mono-sum L+R into all 6 lines. false: left channel only.
    void setStereoInput(bool stereo) { stereoInput_ = stereo; }

    // Holds/repeats the currently-recirculating content by muting new
    // input into the lines - matches the manual's Repeat control.
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

        auto input = stereoInput_ ? 0.5f * (dryLeft + dryRight) : dryLeft;
        if (repeat_)
        {
            input = 0.0f;
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (auto& line : lines_)
        {
            auto out = line.process(input);
            wetLeft += out.left;
            wetRight += out.right;
        }

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(dryRight, wetRight, mix_);
    }

  private:
    void applyLineDelay(int line)
    {
        auto idx = static_cast<std::size_t>(line);
        lines_[idx].setBaseDelaySamples(baseDelayMs_[idx] * masterDelay_ * 0.001f * sampleRate_);
    }
    void applyLineRate(int line)
    {
        auto idx = static_cast<std::size_t>(line);
        lines_[idx].setSweepRate(baseRate_[idx] * masterRate_);
    }
    void applyLineDepth(int line)
    {
        auto idx = static_cast<std::size_t>(line);
        lines_[idx].setSweepDepth(baseDepth_[idx] * masterDepth_);
    }
    void applyLineFeedback(int line)
    {
        auto idx = static_cast<std::size_t>(line);
        lines_[idx].setFeedback(baseFeedback_[idx] * masterFeedback_);
    }
    void applyLinePan(int line)
    {
        auto idx = static_cast<std::size_t>(line);
        lines_[idx].setPan(std::clamp(basePan_[idx] * width_, -1.0f, 1.0f));
    }

    std::array<SweptCombVoice, kNumLines> lines_;
    std::array<float, kNumLines> baseDelayMs_{};
    std::array<float, kNumLines> baseRate_{};
    std::array<float, kNumLines> baseDepth_{};
    std::array<float, kNumLines> baseFeedback_{};
    std::array<float, kNumLines> basePan_{};

    float sampleRate_ = 48000.0f;
    float masterDelay_ = 1.0f;
    float masterRate_ = 0.5f;
    float masterDepth_ = 0.5f;
    float masterFeedback_ = 1.0f;
    float width_ = 1.0f;
    float mix_ = 0.5f;
    bool stereoInput_ = true;
    bool repeat_ = false;
};
}
