#pragma once

#include "dsp/Allpass.h"
#include "dsp/DelayLine.h"
#include "dsp/FeedbackMatrix.h"
#include "dsp/LFO.h"
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
 * A Lexicon PCM81-inspired hall reverb.
 *
 * Topology: a short chain of series allpass filters diffuses the input
 * into a dense cluster of echoes, which then drives an 8-line feedback
 * delay network mixed every sample by a lossless Householder matrix. Each
 * line has its own one-pole damping filter (high-frequency loss) and its
 * own feedback gain, chosen so all eight lines decay to -60dB at the same
 * user-set time despite having different lengths. Two of the eight lines
 * are gently, independently modulated so the tail keeps moving instead of
 * settling into an audibly periodic, metallic ring.
 *
 * This models the *style* of Lexicon's classic algorithmic reverbs (dense
 * mutually-prime-length diffusion/FDN, damped feedback, subtle tank
 * modulation) - it is an original implementation, not a reverse-engineered
 * copy of Lexicon's proprietary algorithm.
 */
class LexiconHall
{
  public:
    static constexpr int kNumDiffusers = 4;
    static constexpr int kNumLines = 8;
    static constexpr int kModMargin = 8;

    static constexpr std::array<int, kNumDiffusers> kDiffuserLengths = { 211, 431, 751, 1091 };
    static constexpr std::array<int, kNumLines> kLineLengths =
      { 977, 1301, 1663, 2063, 2521, 3037, 3643, 4357 };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        std::size_t total = 0;
        for (auto length : kDiffuserLengths)
        {
            total += static_cast<std::size_t>(length);
        }
        for (auto length : kLineLengths)
        {
            total += static_cast<std::size_t>(length) + kModMargin;
        }
        return total;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        for (int i = 0; i < kNumDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kDiffuserLengths[i]);
            diffusers_[i].setBuffer(workingBuffer.subspan(offset, length));
            diffusers_[i].setCoefficient(0.6f);
            offset += length;
        }

        for (int i = 0; i < kNumLines; ++i)
        {
            auto length = static_cast<std::size_t>(kLineLengths[i]) + kModMargin;
            lines_[i].setBuffer(workingBuffer.subspan(offset, length));
            offset += length;
        }

        // Slow, mutually-detuned modulation on alternating lines only, so
        // the tank stays lively without smearing pitch on transients.
        static constexpr std::array<float, kNumLines> modRatesHz =
          { 0.083f, 0.0f, 0.13f, 0.0f, 0.19f, 0.0f, 0.11f, 0.0f };
        for (int i = 0; i < kNumLines; ++i)
        {
            lfo_[i].setFrequency(modRatesHz[i], sampleRate_);
            lfo_[i].setPhase(static_cast<float>(i) / static_cast<float>(kNumLines));
        }

        setDecaySeconds(2.0f);
        setDamping(0.5f);
        setMix(0.35f);
        reset();
    }

    // Approximate RT60 (time to decay by 60dB), in seconds.
    void setDecaySeconds(float seconds)
    {
        auto clamped = std::max(seconds, 0.05f);
        for (int i = 0; i < kNumLines; ++i)
        {
            auto lineSeconds = static_cast<float>(kLineLengths[i]) / sampleRate_;
            feedbackGain_[i] = std::pow(10.0f, -3.0f * lineSeconds / clamped);
        }
    }

    // 0 (bright/undamped) .. 1 (dark, fast high-frequency decay).
    void setDamping(float amount)
    {
        auto coefficient = mapLinear(amount, 0.05f, 0.85f);
        for (auto& d : damping_)
        {
            d.setCoefficient(coefficient);
        }
    }

    // 0 (fully dry) .. 1 (fully wet).
    void setMix(float wet) { wet_ = clamp01(wet); }

    // While frozen, the tank recirculates near-losslessly and dry input is
    // excluded from it, so whatever is already ringing sustains indefinitely.
    void setFrozen(bool frozen) { frozen_ = frozen; }

    void reset()
    {
        for (auto& d : diffusers_)
        {
            d.reset();
        }
        for (auto& l : lines_)
        {
            l.reset();
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
            auto dry = 0.5f * (left[n] + right[n]);

            auto diffused = dry;
            for (auto& d : diffusers_)
            {
                diffused = d.process(diffused);
            }
            if (frozen_)
            {
                diffused = 0.0f;
            }

            std::array<float, kNumLines> tapped{};
            for (int i = 0; i < kNumLines; ++i)
            {
                auto modulated = isModulated(i);
                auto modValue = modulated ? lfo_[i].nextSine() : 0.0f;
                auto delaySamples = static_cast<float>(kLineLengths[i]) + modValue * 3.0f;
                tapped[i] = lines_[i].readLinear(delaySamples);
            }

            std::array<float, kNumLines> damped{};
            for (int i = 0; i < kNumLines; ++i)
            {
                damped[i] = damping_[i].process(tapped[i]);
            }

            householderMix(damped);

            static constexpr std::array<float, kNumLines> inputSign =
              { 1, -1, 1, -1, 1, -1, 1, -1 };
            for (int i = 0; i < kNumLines; ++i)
            {
                auto feedbackGain = frozen_ ? 0.9999f : feedbackGain_[i];
                auto lineInput = diffused * inputSign[i] * 0.5f + damped[i] * feedbackGain;
                lines_[i].write(lineInput);
            }

            // Decorrelated stereo taps: alternating line groups summed with
            // different signs, matching common FDN reverb output stages.
            float wetLeft = 0.0f;
            float wetRight = 0.0f;
            for (int i = 0; i < kNumLines; ++i)
            {
                auto tap = tapped[i];
                wetLeft += tap;
                wetRight += (i % 2 == 0) ? -tap : tap;
            }
            wetLeft *= 0.35f;
            wetRight *= 0.35f;

            left[n] = lerp(left[n], wetLeft, wet_);
            right[n] = lerp(right[n], wetRight, wet_);
        }
    }

  private:
    static bool isModulated(int index) { return index % 2 == 0; }

    float sampleRate_ = 48000.0f;
    float wet_ = 0.35f;
    bool frozen_ = false;

    std::array<Allpass, kNumDiffusers> diffusers_;
    std::array<DelayLine, kNumLines> lines_;
    std::array<OnePoleLowpass, kNumLines> damping_;
    std::array<LFO, kNumLines> lfo_;
    std::array<float, kNumLines> feedbackGain_{};
};
}
