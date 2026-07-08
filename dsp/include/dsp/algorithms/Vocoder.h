#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/StateVariableFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Vocoder" (Algorithm 115), per that
 * algorithm's own manual page: "A vocoder is used to impress the
 * articulatory characteristics of one instrument onto the timbre and
 * pitch of another. Usually the articulation information (the
 * 'analysis' input) comes from a spoken voice, while the timbre and
 * pitch come from a keyboard, guitar, or any other instrument (the
 * 'synthesis' input)." Right input = analysis (voice), Left input =
 * synthesis (instrument), per the manual's own channel convention.
 *
 * Built as a classic multi-band channel vocoder: a bank of bandpass
 * filters (reusing `StateVariableFilter`, already built for Patch
 * Factory's own tuneable filters) analyzes the voice input's per-band
 * energy, and an identically-shaped synthesis filterbank processing the
 * instrument input is scaled by those energies band-by-band before
 * summing - the standard way to build a vocoder's "impress one sound's
 * spectral envelope onto another's" behavior. The real PEL firmware
 * almost certainly used a more exotic (possibly LPC-style, given
 * "Min Error" and "Max Resonance" expert parameters that read like an
 * adaptive-filter tracking criterion) approach, but that isn't public,
 * so this is an original reconstruction of the *described* character
 * rather than a reverse-engineered internal algorithm - consistent with
 * every other H3000/PCM81 Block in this archive whose manual describes
 * behavior without specifying internals.
 *
 * Formant Speed and Envelope Speed are modeled as two cascaded one-pole
 * smoothing stages on each band's analysis envelope (matching the
 * manual's own two-speed-parameter structure: "speed at which the
 * synthesis filter tracks the spectrum" vs. "tracks the articulation")
 * rather than as a real adaptive/LPC filter.
 */
class Vocoder
{
  public:
    static constexpr int kNumBands = 12;
    static constexpr float kLowBandHz = 90.0f;
    static constexpr float kHighBandHz = 5000.0f;
    static constexpr float kMaxWidthSeconds = 0.01f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kWidthCapacitySamples =
      static_cast<std::size_t>(kMaxWidthSeconds * kMaxSampleRate) + 2;

    static constexpr std::size_t requiredWorkingBufferSize() { return kWidthCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        widthLine_.setBuffer(workingBuffer.subspan(0, kWidthCapacitySamples));

        auto ratio = std::pow(kHighBandHz / kLowBandHz, 1.0f / static_cast<float>(kNumBands - 1));
        for (int i = 0; i < kNumBands; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            bandCenterHz_[idx] = kLowBandHz * std::pow(ratio, static_cast<float>(i));
            analysisFilters_[idx].prepare(sampleRate_);
            synthesisFilters_[idx].prepare(sampleRate_);
        }

        setFormantSpeed(50.0f);
        setEnvelopeSpeed(50.0f);
        setFormantShift(0.0f);
        setDepth(0.5f);
        setWidthSeconds(0.005f);
        setMix(1.0f);
        setMaxResonance(30.0f);
        setThreshold(0.02f);

        reset();
    }

    // -- Core controls (#0-5) --
    void setFormantSpeed(float speed0to100)
    {
        auto hz = mapLinear(clamp01(speed0to100 / 100.0f), 2.0f, 60.0f);
        formantCoefficient_ = onePoleLowpassCoefficient(hz, sampleRate_);
    }
    void setEnvelopeSpeed(float speed0to100)
    {
        auto hz = mapLinear(clamp01(speed0to100 / 100.0f), 2.0f, 60.0f);
        envelopeCoefficient_ = onePoleLowpassCoefficient(hz, sampleRate_);
    }
    // 0..100: shifts the synthesis filterbank's center frequencies
    // upward, "munchkin-izing" the vocoded sound at high settings.
    void setFormantShift(float amount0to100)
    {
        formantShiftMultiplier_ = 1.0f + clamp01(amount0to100 / 100.0f) * 1.5f;
    }
    // 0 (mono) .. 1 (full pseudo-stereo depth).
    void setDepth(float amount0to1) { depth_ = clamp01(amount0to1); }
    // 0..10ms pseudo-stereo image width.
    void setWidthSeconds(float seconds)
    {
        widthSamples_ = std::clamp(seconds, 0.0f, kMaxWidthSeconds) * sampleRate_;
    }
    void setMix(float wet) { mix_ = clamp01(wet); }

    // -- Expert controls (#6-8) --
    // 0..100: how "ringy" (high-Q) the filterbank is allowed to get.
    void setMaxResonance(float amount0to100)
    {
        auto q = clamp01(amount0to100 / 100.0f) * 0.9f;
        for (auto& f : analysisFilters_)
        {
            f.setQ(q);
        }
        for (auto& f : synthesisFilters_)
        {
            f.setQ(q);
        }
    }
    // 0..1: analysis input level below which the vocoder gates shut
    // (the manual's own built-in noise gate, "eliminates mis-tracking
    // caused by input noise or hum").
    void setThreshold(float level0to1) { threshold_ = clamp01(level0to1); }

    void reset()
    {
        for (auto& f : analysisFilters_)
        {
            f.reset();
        }
        for (auto& f : synthesisFilters_)
        {
            f.reset();
        }
        for (auto& s : formantSmoothers_)
        {
            s.reset();
        }
        for (auto& s : envelopeSmoothers_)
        {
            s.reset();
        }
        gateEnvelope_.reset();
        widthLine_.reset();
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    // Left = synthesis (instrument) input, Right = analysis (voice)
    // input, per the manual's own channel convention.
    void processSample(float& left, float& right)
    {
        auto synthesisIn = left;
        auto analysisIn = right;

        gateEnvelope_.setCoefficient(onePoleLowpassCoefficient(15.0f, sampleRate_));
        auto analysisLevel = gateEnvelope_.process(std::fabs(analysisIn));
        auto gateOpen = analysisLevel > threshold_;

        float vocoded = 0.0f;
        for (int i = 0; i < kNumBands; ++i)
        {
            auto idx = static_cast<std::size_t>(i);

            analysisFilters_[idx].setCutoff(bandCenterHz_[idx]);
            auto analysisBand = analysisFilters_[idx].process(analysisIn).bandpass;
            auto rectified = std::fabs(analysisBand);

            formantSmoothers_[idx].setCoefficient(formantCoefficient_);
            auto stage1 = formantSmoothers_[idx].process(rectified);
            envelopeSmoothers_[idx].setCoefficient(envelopeCoefficient_);
            auto gain = envelopeSmoothers_[idx].process(stage1);
            if (!gateOpen)
            {
                gain = 0.0f;
            }

            auto synthesisCenterHz =
              std::min(bandCenterHz_[idx] * formantShiftMultiplier_, sampleRate_ / 6.0f);
            synthesisFilters_[idx].setCutoff(synthesisCenterHz);
            auto synthesisBand = synthesisFilters_[idx].process(synthesisIn).bandpass;

            vocoded += synthesisBand * gain;
        }
        vocoded *= kBandNormalization;

        widthLine_.write(vocoded);
        auto delayed = widthLine_.readLinear(widthSamples_);

        auto wetLeft = vocoded;
        auto wetRight = lerp(vocoded, delayed, depth_);

        left = lerp(synthesisIn, wetLeft, mix_);
        right = lerp(synthesisIn, wetRight, mix_);
    }

  private:
    static constexpr float kBandNormalization = 6.0f;

    float sampleRate_ = 48000.0f;

    std::array<float, kNumBands> bandCenterHz_{};
    std::array<StateVariableFilter, kNumBands> analysisFilters_;
    std::array<StateVariableFilter, kNumBands> synthesisFilters_;
    std::array<OnePoleLowpass, kNumBands> formantSmoothers_;
    std::array<OnePoleLowpass, kNumBands> envelopeSmoothers_;
    float formantCoefficient_ = 0.0f;
    float envelopeCoefficient_ = 0.0f;
    float formantShiftMultiplier_ = 1.0f;

    OnePoleLowpass gateEnvelope_;
    float threshold_ = 0.02f;

    DelayLine widthLine_;
    float widthSamples_ = 0.0f;
    float depth_ = 0.5f;

    float mix_ = 1.0f;
};
}
