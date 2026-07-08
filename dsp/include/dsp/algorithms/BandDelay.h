#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/StereoRotate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Band Delay" (Algorithm 117), per that
 * algorithm's own manual page: "This algorithm is a multi-tap delay
 * line, with each of its eight delay outputs connected to a separate
 * bandpass filter. The outputs of the eight bandpass filters are
 * combined in a stereo mixer." The manual's own singular phrasing ("a
 * multi-tap delay line," not "eight delay lines") is taken literally
 * here: one shared mono `DelayLine`, written from the summed stereo
 * input plus a recirculating feedback loop, with 8 independently
 * runtime-settable read taps - reusing `StateVariableFilter` (already
 * built for Patch Factory and Vocoder) as each tap's bandpass filter.
 *
 * Tuning per filter is the sum of three sources, per the manual's own
 * worked example (Global Frequency + individual Frequency cents +
 * individual Note, converted to Hz via semitone math): this Block
 * exposes Global Frequency (semitones, applied to all 8 filters
 * together) and per-filter Frequency (cents), applied on top of a
 * settable per-filter base Hz - a direct Hz value standing in for the
 * manual's Note parameter, since no MIDI keyboard input pathway exists
 * in any of this project's targets (Polyend Endless, native host, JUCE
 * plugin) to drive note-name tuning or the manual's own Note Mode
 * (#57: off/routed/ordered/circular), which is skipped entirely for the
 * same reason - see docs/eventide-band-delay.md.
 *
 * Global Q Factor (0-100%) scales each filter's own raw Q setting
 * (0-999) multiplicatively, matching the manual's own stated normal
 * usage: "these are all set to 999 and the global Q factor is used to
 * adjust the value of all the Q factors simultaneously."
 */
class BandDelay
{
  public:
    static constexpr int kNumBands = 8;
    static constexpr float kMaxDelaySeconds = 1.5f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kLineCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize() { return 2 * kLineCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        sharedLine_.setBuffer(workingBuffer.subspan(0, kLineCapacitySamples));
        feedbackLine_.setBuffer(workingBuffer.subspan(kLineCapacitySamples, kLineCapacitySamples));

        static constexpr std::array<float, kNumBands> kDefaultBaseHz = { 110.0f,  220.0f,  330.0f, 440.0f,
                                                                          660.0f,  880.0f,  1320.0f, 2200.0f };
        static constexpr std::array<float, kNumBands> kDefaultDelaysMs = { 83, 149, 227, 311, 401, 487, 571, 661 };
        static constexpr std::array<float, kNumBands> kDefaultPans = { -1.0f, 1.0f, -0.6f, 0.6f,
                                                                         -0.3f, 0.3f, -0.1f, 0.1f };
        for (int i = 0; i < kNumBands; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            baseHz_[idx] = kDefaultBaseHz[idx];
            frequencyCents_[idx] = 0.0f;
            qRaw_[idx] = 999.0f;
            delayMs_[idx] = kDefaultDelaysMs[idx];
            level_[idx] = (i % 2 == 0) ? 1.0f : -1.0f; // alternating +/- per the manual's own Hint
            pan_[idx] = kDefaultPans[idx];
            filters_[idx].prepare(sampleRate_);
        }

        setGlobalDelay(100.0f);
        setGlobalFrequency(0.0f);
        setGlobalQ(100.0f);
        setGlobalPan(0.0f);
        setFeedbackDelaySeconds(0.3f);
        setFeedback(0.0f);
        setMix(0.5f);

        reset();
    }

    // -- Core controls (#0-3, #8, #58-59) --
    // 0-100%: master scale applied to all eight individual Delay values.
    void setGlobalDelay(float percent0to100) { globalDelayScale_ = clamp01(percent0to100 / 100.0f); }
    // -128..128 notes (semitones): added to every filter's tuning.
    void setGlobalFrequency(float semitones) { globalFrequencySemitones_ = std::clamp(semitones, -128.0f, 128.0f); }
    // 0-100%: master scale applied to all eight individual Q settings.
    void setGlobalQ(float percent0to100) { globalQScale_ = clamp01(percent0to100 / 100.0f); }
    // -1 (L-R) .. +1 (R-L): stereo field width, reusing rotateStereoWidth.
    void setGlobalPan(float amountMinus1to1) { globalPanDegrees_ = std::clamp(amountMinus1to1, -1.0f, 1.0f) * 180.0f; }
    void setFeedbackDelaySeconds(float seconds)
    {
        feedbackDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    // -100..100%: negative inverts phase in the feedback path.
    void setFeedback(float percent) { feedback_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    void setMix(float wet) { mix_ = clamp01(wet); }

    // -- Expert per-filter controls (#9-56) --
    // A direct Hz value standing in for the manual's Note parameter -
    // see the class doc comment for why.
    void setFilterBaseHz(int band, float hz) { baseHz_[static_cast<std::size_t>(band)] = std::max(hz, 1.0f); }
    // 0-12800 cents, added on top of the base Hz (and Global Frequency).
    void setFilterFrequencyCents(int band, float cents)
    {
        frequencyCents_[static_cast<std::size_t>(band)] = std::clamp(cents, 0.0f, 12800.0f);
    }
    // 0-999 raw Q, scaled by Global Q before reaching the filter.
    void setFilterQ(int band, float q0to999) { qRaw_[static_cast<std::size_t>(band)] = std::clamp(q0to999, 0.0f, 999.0f); }
    void setFilterDelayMs(int band, float ms) { delayMs_[static_cast<std::size_t>(band)] = std::clamp(ms, 0.0f, 1496.0f); }
    // -100..100%; negative inverts phase.
    void setFilterLevel(int band, float percent)
    {
        level_[static_cast<std::size_t>(band)] = std::clamp(percent, -100.0f, 100.0f) / 100.0f;
    }
    void setFilterPan(int band, float panMinus1to1) { pan_[static_cast<std::size_t>(band)] = std::clamp(panMinus1to1, -1.0f, 1.0f); }

    void reset()
    {
        sharedLine_.reset();
        feedbackLine_.reset();
        for (auto& f : filters_)
        {
            f.reset();
        }
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

        auto feedbackContribution = feedbackLine_.readLinear(feedbackDelaySamples_) * feedback_;
        sharedLine_.write(dry + feedbackContribution);

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (int i = 0; i < kNumBands; ++i)
        {
            auto idx = static_cast<std::size_t>(i);

            auto delaySamples = delayMs_[idx] * 0.001f * sampleRate_ * globalDelayScale_;
            auto tapped = sharedLine_.readLinear(delaySamples);

            auto hz = baseHz_[idx] *
                      std::pow(2.0f, (frequencyCents_[idx] / 100.0f + globalFrequencySemitones_) / 12.0f);
            filters_[idx].setCutoff(hz);
            filters_[idx].setQ(clamp01((qRaw_[idx] / 999.0f) * globalQScale_));
            auto filtered = filters_[idx].process(tapped).bandpass;

            auto leveled = filtered * level_[idx];
            auto panL = 0.5f * (1.0f - pan_[idx]);
            auto panR = 0.5f * (1.0f + pan_[idx]);
            wetLeft += leveled * panL;
            wetRight += leveled * panR;
        }
        wetLeft *= kBandNormalization;
        wetRight *= kBandNormalization;

        rotateStereoWidth(wetLeft, wetRight, globalPanDegrees_);

        feedbackLine_.write(0.5f * (wetLeft + wetRight));

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(dryRight, wetRight, mix_);
    }

  private:
    static constexpr float kBandNormalization = 0.5f;

    float sampleRate_ = 48000.0f;

    DelayLine sharedLine_;
    DelayLine feedbackLine_;
    std::array<StateVariableFilter, kNumBands> filters_;

    std::array<float, kNumBands> baseHz_{};
    std::array<float, kNumBands> frequencyCents_{};
    std::array<float, kNumBands> qRaw_{};
    std::array<float, kNumBands> delayMs_{};
    std::array<float, kNumBands> level_{};
    std::array<float, kNumBands> pan_{};

    float globalDelayScale_ = 1.0f;
    float globalFrequencySemitones_ = 0.0f;
    float globalQScale_ = 1.0f;
    float globalPanDegrees_ = 0.0f;
    float feedbackDelaySamples_ = 0.0f;
    float feedback_ = 0.0f;
    float mix_ = 0.5f;
};
}
