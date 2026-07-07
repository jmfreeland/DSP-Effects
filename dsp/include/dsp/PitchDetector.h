#pragma once

#include "dsp/DelayLine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp
{
/**
 * Real-time monophonic pitch (fundamental frequency) tracker, via
 * normalized autocorrelation over a sliding window - the building block
 * an H3000-style "Diatonic Shift" needs to actually track the input note
 * (see docs/eventide-diatonic-shift.md; this Primitive exists because the
 * real algorithm's own description states plainly "the H3000 tracks your
 * pitch and plays the correct notes," which the first version of that
 * engine didn't do at all).
 *
 * Runs a correlation search once per hop (not every sample - the search
 * is the expensive part) across lags corresponding to [minHz, maxHz],
 * takes the lag with the strongest normalized correlation as the
 * detected period, and reports 0 (via hasPitch()) when no lag clears the
 * confidence threshold (silence, noise, or a signal outside the search
 * range). This is a standard, well-understood technique (the same family
 * as YIN), not a from-scratch invention - chosen over zero-crossing
 * counting (too fragile against harmonics/noise) and over an FFT/
 * phase-vocoder approach (autocorrelation needs no transform, fitting
 * the same no-allocation constraints as the rest of dsp/).
 */
class PitchDetector
{
  public:
    static constexpr float kDefaultMinHz = 60.0f;
    static constexpr float kDefaultMaxHz = 1000.0f;

    void setBuffer(std::span<float> buffer) { history_.setBuffer(buffer); }

    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;
        hopSamples_ = static_cast<int>(0.01f * sampleRate); // ~10ms between detections
        reset();
    }

    // Search range for the fundamental. Narrower ranges are cheaper (less
    // lag to search) and more robust (matches the real algorithm's own
    // Low Note/High Note "pitch tracking optimization" controls).
    void setFrequencyRange(float minHz, float maxHz)
    {
        minHz_ = std::max(minHz, 20.0f);
        maxHz_ = std::max(maxHz, minHz_ + 1.0f);
    }

    void reset()
    {
        history_.reset();
        hopCounter_ = 0;
        frequencyHz_ = 0.0f;
        confidence_ = 0.0f;
    }

    // Feed one input sample; periodically (every hop) re-runs detection.
    void process(float input)
    {
        history_.write(input);
        if (++hopCounter_ >= hopSamples_)
        {
            hopCounter_ = 0;
            detect();
        }
    }

    // Most recently detected fundamental, in Hz. Holds its last value
    // through unvoiced/silent stretches rather than snapping to 0, so a
    // downstream harmonizer doesn't glitch between notes.
    float frequencyHz() const { return frequencyHz_; }

    // Whether the most recent detection cleared the confidence threshold
    // (a real, sustained periodic signal was present).
    bool hasPitch() const { return confidence_ > kConfidenceThreshold; }

  private:
    // Normalized autocorrelation at a single lag, over the current window.
    float correlationAt(int lag, std::size_t windowSize) const
    {
        auto count = windowSize - static_cast<std::size_t>(lag);
        float sumProduct = 0.0f;
        float sumSqA = 0.0f;
        float sumSqB = 0.0f;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto a = history_.read(i);
            auto b = history_.read(i + static_cast<std::size_t>(lag));
            sumProduct += a * b;
            sumSqA += a * a;
            sumSqB += b * b;
        }
        auto denom = std::sqrt(sumSqA * sumSqB);
        return denom > 1e-9f ? sumProduct / denom : 0.0f;
    }

    void detect()
    {
        auto minLag = static_cast<int>(sampleRate_ / maxHz_);
        auto maxLag = static_cast<int>(sampleRate_ / minHz_);
        minLag = std::max(minLag, 1);
        auto windowSize = static_cast<std::size_t>(maxLag) * 2;
        windowSize = std::min(windowSize, history_.size());
        if (windowSize <= static_cast<std::size_t>(maxLag))
        {
            return; // not enough history yet to search this far
        }

        auto bestCorrelation = 0.0f;
        auto bestLag = 0;
        for (auto lag = minLag; lag <= maxLag; ++lag)
        {
            auto correlation = correlationAt(lag, windowSize);
            if (correlation > bestCorrelation)
            {
                bestCorrelation = correlation;
                bestLag = lag;
            }
        }

        // A pure or harmonically simple tone often correlates almost as
        // strongly at an integer submultiple of its true period (e.g. a
        // clean sine at half the lag = twice the frequency), which biases
        // a plain global-max search an octave too low. Prefer the
        // shortest lag that's still nearly as confident, halving
        // repeatedly while it holds - a standard octave-error mitigation.
        while (bestLag / 2 >= minLag)
        {
            auto halfLag = bestLag / 2;
            auto halfCorrelation = correlationAt(halfLag, windowSize);
            if (halfCorrelation < bestCorrelation * 0.9f)
            {
                break;
            }
            bestLag = halfLag;
            bestCorrelation = halfCorrelation;
        }

        confidence_ = bestCorrelation;
        if (bestLag > 0 && confidence_ > kConfidenceThreshold)
        {
            auto detected = sampleRate_ / static_cast<float>(bestLag);
            frequencyHz_ =
              frequencyHz_ <= 0.0f ? detected : frequencyHz_ + kSmoothing * (detected - frequencyHz_);
        }
    }

    static constexpr float kConfidenceThreshold = 0.6f;
    static constexpr float kSmoothing = 0.35f;

    DelayLine history_;
    float sampleRate_ = 48000.0f;
    float minHz_ = kDefaultMinHz;
    float maxHz_ = kDefaultMaxHz;
    int hopSamples_ = 480;
    int hopCounter_ = 0;
    float frequencyHz_ = 0.0f;
    float confidence_ = 0.0f;
};
}
