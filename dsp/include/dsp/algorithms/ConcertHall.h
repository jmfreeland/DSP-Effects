#pragma once

#include "dsp/Crossover.h"
#include "dsp/Decay.h"
#include "dsp/DelayLine.h"
#include "dsp/Diffuser.h"
#include "dsp/Envelope.h"
#include "dsp/FeedbackMatrix.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * A Lexicon PCM81-inspired "Concert Hall": clean reverberation designed to
 * stay behind the source, with initial echo density that builds up
 * gradually rather than arriving all at once.
 *
 * Topology (mirrors the PCM81 user guide's reverb-core block diagram):
 *
 *   input -> PreDelay -+-> Diffusion (series allpass chain) -> tank input
 *                       +-> early-reflection tap (RefDly/RefLvl) -> output
 *
 *   8-line Householder FDN tank, per line:
 *     tapped   = delay.read(length * Size +/- Spin/Chorus wobble)
 *     damped   = onePoleLowpass(tapped)              // Rt HC
 *     low,high = crossover(damped)                   // split at Crossover Hz
 *     decayed  = low*lowGain + high*midGain           // Low Rt / Mid Rt
 *   householderMix(decayed across all 8 lines)
 *   delay.write(diffused input + decayed)
 *
 * This models the *style* of Lexicon's Concert Hall algorithm (dense
 * prime-length diffusion/FDN, independently-decaying low/mid bands, early
 * reflections ahead of a damped tail, gentle modulation to avoid a
 * metallic ring) - it is an original implementation, not a
 * reverse-engineered copy. See docs/lexicon-pcm81-reference.md for the
 * primary-source parameter definitions this is built from, and
 * docs/lexicon-pcm81-hall.md for the remaining gaps against it.
 */
class ConcertHall
{
  public:
    static constexpr int kNumDiffusers = 4;
    static constexpr int kNumLines = 8;
    static constexpr int kModMargin = 16;

    // Pre-delay and early-reflection capacities are sized for 48kHz (the
    // Endless's native rate) regardless of the sample rate passed to
    // prepare() - the same simplification already used for tank lengths.
    static constexpr int kPreDelayCapacitySamples = 44650;       // ~930ms @ 48kHz
    static constexpr int kEarlyReflectionCapacitySamples = 57600; // 1.2s @ 48kHz

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
        total += kPreDelayCapacitySamples;
        total += kEarlyReflectionCapacitySamples;
        return total;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        for (int i = 0; i < kNumDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kDiffuserLengths[i]);
            diffuser_.setStageBuffer(i, workingBuffer.subspan(offset, length));
            offset += length;
        }

        for (int i = 0; i < kNumLines; ++i)
        {
            auto length = static_cast<std::size_t>(kLineLengths[i]) + kModMargin;
            lines_[i].setBuffer(workingBuffer.subspan(offset, length));
            offset += length;
        }

        preDelayLine_.setBuffer(workingBuffer.subspan(offset, kPreDelayCapacitySamples));
        offset += kPreDelayCapacitySamples;

        earlyReflectionLine_.setBuffer(
          workingBuffer.subspan(offset, kEarlyReflectionCapacitySamples));
        offset += kEarlyReflectionCapacitySamples;

        // Spin (slow, tail-wide movement) and Chorus (faster, per-line
        // delay randomization to kill metallic ringing) are split across
        // alternating lines rather than both applied everywhere - simpler,
        // and still gives every line some liveliness.
        static constexpr std::array<float, kNumLines> spinRatesHz =
          { 0.083f, 0.0f, 0.13f, 0.0f, 0.19f, 0.0f, 0.11f, 0.0f };
        static constexpr std::array<float, kNumLines> chorusRatesHz =
          { 0.0f, 0.6f, 0.0f, 0.9f, 0.0f, 1.3f, 0.0f, 0.7f };
        for (int i = 0; i < kNumLines; ++i)
        {
            spinLfo_[i].setFrequency(spinRatesHz[i], sampleRate_);
            spinLfo_[i].setPhase(static_cast<float>(i) / static_cast<float>(kNumLines));
            chorusLfo_[i].setFrequency(chorusRatesHz[i], sampleRate_);
            chorusLfo_[i].setPhase(static_cast<float>(i) / static_cast<float>(kNumLines) + 0.37f);
        }

        setDiffusion(0.6f);
        setSize(1.0f);
        setDecaySeconds(2.5f);
        setLowRatio(1.0f);
        setCrossoverFrequency(400.0f);
        setDamping(0.5f);
        setPreDelaySeconds(0.0f);
        setEarlyReflectionLevel(0.2f);
        setEarlyReflectionDelaySeconds(0.03f);
        setSpin(0.5f);
        setChorus(0.3f);
        setMix(0.35f);
        reset();
    }

    // Master reverb time (RT60 for mid/high frequencies), in seconds.
    void setDecaySeconds(float seconds)
    {
        decaySeconds_ = std::max(seconds, 0.05f);
        updateDecayGains();
    }

    // Multiplier of decaySeconds() for low-frequency decay (Lexicon
    // recommends <=1.5 for a natural-sounding hall).
    void setLowRatio(float ratio)
    {
        lowRatio_ = std::max(ratio, 0.05f);
        updateDecayGains();
    }

    // Frequency (Hz) where decay hands off from the low-band rate to the
    // mid-band rate.
    void setCrossoverFrequency(float hz)
    {
        crossoverHz_ = hz;
        for (auto& c : crossover_)
        {
            c.setFrequency(crossoverHz_, sampleRate_);
        }
    }

    // 0 (bright/undamped) .. 1 (dark) - a single-pole high-cut on the
    // reverberated signal (Rt HC), independent of the low/mid split.
    void setDamping(float amount)
    {
        auto hz = mapLinear(1.0f - clamp01(amount), 1000.0f, 20000.0f);
        for (auto& d : damping_)
        {
            d.setCoefficient(onePoleLowpassCoefficient(hz, sampleRate_));
        }
    }

    // 0 (no diffusion) .. 1 (maximum initial echo density).
    void setDiffusion(float amount) { diffuser_.setDiffusion(amount); }

    // 0 (small room) .. 1 (large hall) - scales the tank's effective delay
    // lengths. Per the source material, changing Size briefly mutes the
    // wet signal rather than clicking.
    void setSize(float sizeNormalized)
    {
        auto newScale = mapLinear(sizeNormalized, 0.4f, 1.0f);
        if (std::fabs(newScale - sizeScale_) > 0.001f)
        {
            sizeMuteEnvelope_.trigger(0.0f, 1.0f, sampleRate_ * 0.03f);
        }
        sizeScale_ = newScale;
        updateDecayGains();
    }

    // Gap between input and the onset of reverberation, 0..0.93s.
    void setPreDelaySeconds(float seconds)
    {
        preDelaySamples_ = std::clamp(seconds * sampleRate_, 0.0f,
                                       static_cast<float>(kPreDelayCapacitySamples - 2));
    }

    // 0..1 level of the single early-reflection tap mixed into the output.
    void setEarlyReflectionLevel(float level) { earlyReflectionLevel_ = clamp01(level); }

    // Delay of the early-reflection tap, 0..1.2s.
    void setEarlyReflectionDelaySeconds(float seconds)
    {
        earlyReflectionDelaySamples_ =
          std::clamp(seconds * sampleRate_, 0.0f,
                     static_cast<float>(kEarlyReflectionCapacitySamples - 2));
    }

    // 0..1: slow, tail-wide timbral movement so the tank doesn't settle
    // into a static, metallic set of resonances.
    void setSpin(float amount) { spinDepth_ = mapLinear(amount, 0.0f, 4.0f); }

    // 0..1: faster, per-line delay-time randomization for the same reason,
    // at a different character than Spin.
    void setChorus(float amount) { chorusDepth_ = mapLinear(amount, 0.0f, 6.0f); }

    // 0 (fully dry) .. 1 (fully wet).
    void setMix(float wet) { wet_ = clamp01(wet); }

    // While frozen, the tank recirculates near-losslessly and dry input is
    // excluded from it, so whatever is already ringing sustains
    // indefinitely (matches Lexicon's own "Infinite" freeze behavior).
    void setFrozen(bool frozen) { frozen_ = frozen; }

    void reset()
    {
        diffuser_.reset();
        for (auto& l : lines_)
        {
            l.reset();
        }
        for (auto& d : damping_)
        {
            d.reset();
        }
        for (auto& c : crossover_)
        {
            c.reset();
        }
        preDelayLine_.reset();
        earlyReflectionLine_.reset();
        sizeMuteEnvelope_.reset();
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            auto dry = 0.5f * (left[n] + right[n]);

            auto preDelayed = preDelayLine_.readLinear(preDelaySamples_);
            preDelayLine_.write(dry);

            auto earlyTap = earlyReflectionLine_.readLinear(earlyReflectionDelaySamples_);
            earlyReflectionLine_.write(dry);

            auto diffused = diffuser_.process(preDelayed);
            if (frozen_)
            {
                diffused = 0.0f;
            }

            std::array<float, kNumLines> tapped{};
            for (int i = 0; i < kNumLines; ++i)
            {
                auto modValue =
                  spinLfo_[i].nextSine() * spinDepth_ + chorusLfo_[i].nextSine() * chorusDepth_;
                auto delaySamples = static_cast<float>(kLineLengths[i]) * sizeScale_ + modValue;
                tapped[i] = lines_[i].readLinear(delaySamples);
            }

            std::array<float, kNumLines> decayed{};
            for (int i = 0; i < kNumLines; ++i)
            {
                auto damped = damping_[i].process(tapped[i]);
                auto bands = crossover_[i].process(damped);
                auto lowGain = frozen_ ? 0.9999f : lowGain_[i];
                auto midGain = frozen_ ? 0.9999f : midGain_[i];
                decayed[i] = bands.low * lowGain + bands.high * midGain;
            }

            householderMix(decayed);

            static constexpr std::array<float, kNumLines> inputSign =
              { 1, -1, 1, -1, 1, -1, 1, -1 };
            for (int i = 0; i < kNumLines; ++i)
            {
                auto lineInput = diffused * inputSign[i] * 0.5f + decayed[i];
                lines_[i].write(lineInput);
            }

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

            wetLeft += earlyTap * earlyReflectionLevel_;
            wetRight += earlyTap * earlyReflectionLevel_;

            auto muteGain = sizeMuteEnvelope_.next();
            wetLeft *= muteGain;
            wetRight *= muteGain;

            left[n] = lerp(left[n], wetLeft, wet_);
            right[n] = lerp(right[n], wetRight, wet_);
        }
    }

  private:
    void updateDecayGains()
    {
        for (int i = 0; i < kNumLines; ++i)
        {
            auto lineLengthSamples = static_cast<float>(kLineLengths[i]) * sizeScale_;
            midGain_[i] = rt60ToGain(lineLengthSamples, sampleRate_, decaySeconds_);
            lowGain_[i] = rt60ToGain(lineLengthSamples, sampleRate_, decaySeconds_ * lowRatio_);
        }
    }

    float sampleRate_ = 48000.0f;
    float wet_ = 0.35f;
    bool frozen_ = false;

    DiffuserChain<kNumDiffusers> diffuser_;
    std::array<DelayLine, kNumLines> lines_;
    std::array<OnePoleLowpass, kNumLines> damping_;
    std::array<Crossover, kNumLines> crossover_;
    std::array<LFO, kNumLines> spinLfo_;
    std::array<LFO, kNumLines> chorusLfo_;
    std::array<float, kNumLines> midGain_{};
    std::array<float, kNumLines> lowGain_{};

    DelayLine preDelayLine_;
    DelayLine earlyReflectionLine_;
    LinearRamp sizeMuteEnvelope_;

    float sizeScale_ = 0.0f;
    float decaySeconds_ = 2.5f;
    float lowRatio_ = 1.0f;
    float crossoverHz_ = 400.0f;
    float preDelaySamples_ = 0.0f;
    float earlyReflectionLevel_ = 0.2f;
    float earlyReflectionDelaySamples_ = 0.0f;
    float spinDepth_ = 0.0f;
    float chorusDepth_ = 0.0f;
};
}
