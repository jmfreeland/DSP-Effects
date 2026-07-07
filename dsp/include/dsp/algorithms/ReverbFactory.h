#pragma once

#include "dsp/Crossover.h"
#include "dsp/DelayLine.h"
#include "dsp/Decay.h"
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
 * An Eventide H3000-inspired "Reverb Factory" - Algorithm 107 (see
 * docs/eventide-h3000-notes.md and docs/eventide-reverb-factory.md).
 * Per the manual: "The amount of user control over critical parameters
 * in the Reverb algorithm makes the H3000 unique. Along with the
 * standard Predelay, Decay and Mix parameters comes a switching Gate and
 * tight control over Delay parameters... Two decay times are also
 * provided. Softer sounds (below the gate threshold) can have one decay
 * time and EQ while loud sounds (above the gate threshold) can have
 * different decay and EQ."
 *
 * Shares the six-delay-line Householder network shape with Swept Reverb
 * (Algorithm 106) - see that Block's own doc comment for why this
 * archive reuses `householderMix()` from the Lexicon PCM81 tank rather
 * than inventing an undocumented "Reverb Network" from scratch - but
 * with fixed (non-swept) per-line delay lengths and, distinctively, a
 * dynamics Gate that crossfades each line's decay gain and tone between
 * two independent settings (On: loud/above-threshold, Off: soft/below)
 * rather than a single fixed decay - directly reusing this archive's
 * existing `rt60ToGain()` (already built for the PCM81 side) and
 * `Crossover` two-band splitting.
 */
class ReverbFactory
{
  public:
    static constexpr int kNumLines = 6;
    static constexpr float kMaxPreDelaySeconds = 0.5f;
    static constexpr float kMaxLineDelaySeconds = 0.12f; // matches the manual's 5000-sample (~113ms) cap
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kPreDelayCapacitySamples =
      static_cast<std::size_t>(kMaxPreDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kLineCapacitySamples =
      static_cast<std::size_t>(kMaxLineDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return kPreDelayCapacitySamples + kNumLines * kLineCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        preDelay_.setBuffer(workingBuffer.subspan(0, kPreDelayCapacitySamples));
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            lines_[idx].setBuffer(workingBuffer.subspan(
              kPreDelayCapacitySamples + idx * kLineCapacitySamples, kLineCapacitySamples));
        }

        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 29, 43, 61, 79, 97, 113 };
        for (int i = 0; i < kNumLines; ++i)
        {
            lineDelayMs_[static_cast<std::size_t>(i)] = kDefaultDelaysMs[static_cast<std::size_t>(i)];
        }

        setPredelaySeconds(0.02f);
        setOnDecaySeconds(2.5f);
        setOffDecaySeconds(1.0f);
        setGateTimeSeconds(1.0f);
        setGateSpeed(50.0f);
        setGateThreshold(0.3f);
        setGateEnabled(true);
        setEqCrossoverHz(2000.0f);
        setOnEqGainDb(0.0f);
        setOffEqGainDb(-6.0f);
        setMix(0.5f);

        envelopeFollower_.setCoefficient(onePoleLowpassCoefficient(20.0f, sampleRate_));
        gateSmoother_.setCoefficient(onePoleLowpassCoefficient(80.0f, sampleRate_));
        reset();
    }

    // -- Delay parameters --
    void setPredelaySeconds(float seconds)
    {
        predelaySamples_ =
          std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kPreDelayCapacitySamples - 2));
    }
    void setLineDelayMs(int line, float ms)
    {
        lineDelayMs_[static_cast<std::size_t>(line)] = std::clamp(ms, 1.0f, 113.0f);
    }

    // -- Gate/Decay parameters --
    void setOnDecaySeconds(float seconds) { onDecaySeconds_ = std::max(seconds, 0.1f); }
    void setOffDecaySeconds(float seconds) { offDecaySeconds_ = std::max(seconds, 0.1f); }
    // 0-25s: how long the gate stays open once triggered (re-triggerable).
    void setGateTimeSeconds(float seconds) { gateHoldSamples_ = std::max(seconds, 0.0f) * sampleRate_; }
    // 0-100: envelope follower response speed - higher tracks faster.
    void setGateSpeed(float speed0to100)
    {
        auto hz = mapLinear(clamp01(speed0to100 / 100.0f), 2.0f, 100.0f);
        envelopeFollower_.setCoefficient(onePoleLowpassCoefficient(hz, sampleRate_));
    }
    // 0..1: input level required to trigger the gate open.
    void setGateThreshold(float threshold0to1) { gateThreshold_ = clamp01(threshold0to1); }
    // If disabled, the reverb always uses the On decay/EQ settings,
    // matching the manual: "If the gate is Disabled the reverb uses only
    // the Gate On EQ settings."
    void setGateEnabled(bool enabled) { gateEnabled_ = enabled; }

    // -- EQ parameters (simplified: a single crossover point per line
    // rather than the manual's independent low-shelf/high-shelf pair -
    // see docs/eventide-reverb-factory.md's "Known simplifications") --
    void setEqCrossoverHz(float hz) { eqCrossoverHz_ = hz; }
    void setOnEqGainDb(float db) { onEqGain_ = std::pow(10.0f, db / 20.0f); }
    void setOffEqGainDb(float db) { offEqGain_ = std::pow(10.0f, db / 20.0f); }

    void setMix(float wet) { mix_ = clamp01(wet); }

    void reset()
    {
        preDelay_.reset();
        for (auto& line : lines_) line.reset();
        for (auto& c : crossovers_) c.reset();
        envelopeFollower_.reset();
        gateSmoother_.reset();
        gateOpen_ = false;
        holdSamplesRemaining_ = 0.0f;
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
        auto dry = 0.5f * (dryLeft + dryRight);

        preDelay_.write(dry);
        auto predelayed = preDelay_.readLinear(predelaySamples_);

        auto envelope = envelopeFollower_.process(std::fabs(dry));
        if (gateEnabled_)
        {
            if (envelope > gateThreshold_)
            {
                gateOpen_ = true;
                holdSamplesRemaining_ = gateHoldSamples_;
            }
            else if (holdSamplesRemaining_ > 0.0f)
            {
                holdSamplesRemaining_ -= 1.0f;
            }
            else
            {
                gateOpen_ = false;
            }
        }
        else
        {
            gateOpen_ = true; // always "On" settings when the gate is disabled.
        }
        auto gateTarget = gateOpen_ ? 1.0f : 0.0f;
        auto gateAmount = gateSmoother_.process(gateTarget);

        for (auto& c : crossovers_)
        {
            c.setFrequency(eqCrossoverHz_, sampleRate_);
        }

        std::array<float, kNumLines> tapped{};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto delaySamples = lineDelayMs_[idx] * 0.001f * sampleRate_;
            tapped[idx] = lines_[idx].readLinear(delaySamples);
        }

        std::array<float, kNumLines> decayed{};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto delaySamples = lineDelayMs_[idx] * 0.001f * sampleRate_;
            auto onGain = rt60ToGain(delaySamples, sampleRate_, onDecaySeconds_);
            auto offGain = rt60ToGain(delaySamples, sampleRate_, offDecaySeconds_);
            auto decayGain = lerp(offGain, onGain, gateAmount);

            auto bands = crossovers_[idx].process(tapped[idx]);
            auto eqGain = lerp(offEqGain_, onEqGain_, gateAmount);
            decayed[idx] = (bands.low + bands.high * eqGain) * decayGain;
        }

        householderMix(decayed);

        static constexpr std::array<float, kNumLines> kInputSign = { 1, -1, 1, -1, 1, -1 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            lines_[idx].write(predelayed * kInputSign[idx] * 0.5f + decayed[idx]);
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
    DelayLine preDelay_;
    std::array<DelayLine, kNumLines> lines_;
    std::array<Crossover, kNumLines> crossovers_;
    std::array<float, kNumLines> lineDelayMs_{};
    OnePoleLowpass envelopeFollower_;
    OnePoleLowpass gateSmoother_;

    float sampleRate_ = 48000.0f;
    float predelaySamples_ = 0.0f;
    float onDecaySeconds_ = 2.5f;
    float offDecaySeconds_ = 1.0f;
    float gateHoldSamples_ = 0.0f;
    float gateThreshold_ = 0.3f;
    bool gateEnabled_ = true;
    bool gateOpen_ = false;
    float holdSamplesRemaining_ = 0.0f;
    float eqCrossoverHz_ = 2000.0f;
    float onEqGain_ = 1.0f;
    float offEqGain_ = 1.0f;
    float mix_ = 0.5f;
};
}
