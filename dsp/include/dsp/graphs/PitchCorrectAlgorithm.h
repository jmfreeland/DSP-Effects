#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/PitchDetector.h"
#include "dsp/PitchShifter.h"
#include "dsp/StereoRotate.h"
#include "dsp/algorithms/Chamber.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Lexicon PCM81 "Pitch Correct" algorithm - the seventh and last of
 * the Pitch class, completing this archive's entire PCM81 roadmap. Per
 * the manual: "The Vocal Fix Pitch Correct algorithm is designed to work
 * with monophonic (one note at a time) vocal sources. The algorithm
 * contains an intelligent pitch shifter combined with a PCM 81 Chamber
 * reverb. The intelligent pitch shifter detects the pitch of incoming
 * audio and produces corrections based on the detected pitch... The
 * reverb follows the pitch shifter in series. The FX Mix parameter is
 * set to 0% reverb as most applications require only pitch processing."
 *
 * Unlike the six other Pitch algorithms, this one has no Submixer at all
 * (a fixed series chain) and, per its own edit matrix (no Key/Scale/Root
 * row - contrast Res2>Plate's genuinely diatonic-harmony pitch row),
 * corrects toward the nearest *chromatic* (equal-tempered) semitone
 * relative to the Tuning reference, not a diatonic scale degree - "notes
 * are shifted as close as possible to the frequency of the detected
 * pitch" is a 12-TET quantizer, simpler than Res2>Plate's Key/Scale/Root/
 * Rule machinery, and doesn't need dsp::DiatonicScale.h at all.
 *
 *   L,R -> InLvl/InPan -> mono sum -> Delay -+-> PitchDetector
 *                                             +-> PitchShifter (correction cents) -+-> Chamber -+
 *                                                                                   |            |
 *                                                                    FX Mix (0 = dry corrected, 1 = reverbed)
 *                                                                                   -> FX Width/Hi-Cut/Adjust -> Mix -> out
 *
 * No dedicated Block: like QuadHallAlgorithm, this Graph reuses Chamber
 * exactly as built (no new reverb-core capability) and owns the
 * detector/shifter/delay directly.
 */
class PitchCorrectAlgorithm
{
  public:
    enum class Tracking
    {
        kFastest,
        kFast,
        kModerate,
        kSlow,
        kHold,
    };

    static constexpr float kMaxDelaySeconds = 0.1f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kDetectorHistorySamples = 8192;
    static constexpr std::size_t kShifterCapacitySamples =
      static_cast<std::size_t>(PitchShifter::kMaxGrainSeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Chamber::requiredWorkingBufferSize() + kDelayCapacitySamples +
               kDetectorHistorySamples + kShifterCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = dsp::algorithms::Chamber::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        delay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        detector_.setBuffer(workingBuffer.subspan(offset, kDetectorHistorySamples));
        offset += kDetectorHistorySamples;
        shifter_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));

        detector_.prepare(sampleRate);
        shifter_.prepare(sampleRate);
        reverb_.setMix(1.0f);

        setInLevel(1.0f, 1.0f);
        setDelaySeconds(0.01f);
        setPitchRange(80.0f, 800.0f);
        setTuning(440.0f);
        setCorrection(1.0f);
        setTracking(Tracking::kFastest);
        setGrainSeconds(0.008f);
        setShiftCents(0.0f);
        setShiftSemitones(0);
        setFxMix(0.0f);
        setFxWidth(0.0f);
        setHiCut(18000.0f);
        setFxAdjustDb(0.0f);
        setMix(1.0f);
        reset();
    }

    // -- Reverb Block pass-throughs (Chamber) --
    void setDecaySeconds(float seconds) { reverb_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { reverb_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    void setDiffusion(float amount) { reverb_.setDiffusion(amount); }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setLink(bool linked) { reverb_.setLink(linked); }
    void setShape(float amount) { reverb_.setShape(amount); }
    void setSpread(float amount) { reverb_.setSpread(amount); }
    void setRvbIn(float level) { reverb_.setRvbIn(level); }
    void setRvbOut(float level) { reverb_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { reverb_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        reverb_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setSpin(float amount) { reverb_.setSpin(amount); }
    void setEkoFeedback(float left, float right) { reverb_.setEkoFeedback(left, right); }
    void setEkoDelaySeconds(float left, float right) { reverb_.setEkoDelaySeconds(left, right); }

    // -- Input conditioning --
    // level -1..1 (magnitude = level, sign = phase) applied to each
    // channel before the internal mono sum. No InPan control: this
    // algorithm's output is derived from a single corrected mono signal
    // (per the manual's own diagram, one Pitch Correct box feeding both
    // reverb inputs equally), and a linear pan law's L/R weights always
    // sum to a constant - so routing L/R through a pan matrix before
    // summing back to mono would be mathematically inert (cancels out
    // exactly), unlike every other Graph's InPan, which stays split into
    // a genuine stereo bus downstream.
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    // 0..0.1s: time between input and the corrected output (also the
    // shifter's own processing time).
    void setDelaySeconds(float seconds)
    {
        delaySamples_ = std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples - 2));
    }

    // Brackets the detector's tracked range (also the manual's own
    // "Low Pitch"/"High Pitch" pitch-shifter latency tradeoff).
    void setPitchRange(float lowHz, float highHz) { detector_.setFrequencyRange(lowHz, highHz); }

    // The reference frequency for pitch "A" - 410-470Hz per the manual, 440 standard.
    void setTuning(float aHz) { tuningHz_ = std::max(aHz, 20.0f); }

    // 0 (no correction) .. 1 (fully corrected to the nearest chromatic semitone).
    void setCorrection(float amount) { correctionAmount_ = clamp01(amount); }

    // How quickly the pitch detector's own reading responds to a change
    // (Fastest/Fast/Moderate/Slow), or kHold to freeze at the last
    // detected pitch - "effectively turning any melody into a pedal tone."
    void setTracking(Tracking tracking)
    {
        tracking_ = tracking;
        float hz;
        switch (tracking)
        {
            case Tracking::kFast:
                hz = 30.0f;
                break;
            case Tracking::kModerate:
                hz = 12.0f;
                break;
            case Tracking::kSlow:
                hz = 4.0f;
                break;
            case Tracking::kFastest:
            case Tracking::kHold:
            default:
                hz = 60.0f;
                break;
        }
        trackingFilter_.setCoefficient(onePoleLowpassCoefficient(hz, sampleRate_));
    }

    void setGrainSeconds(float seconds) { shifter_.setGrainSeconds(seconds); }

    // An additional fixed transpose on top of correction, per the
    // manual's own "Shift Cents"/"Shift Semitones" (additive to +-2 octaves).
    void setShiftCents(float cents) { shiftCents_ = cents; }
    void setShiftSemitones(int semitones) { shiftSemitones_ = semitones; }

    void setFxMix(float mix) { fxMix_ = clamp01(mix); }
    void setFxWidth(float degrees) { fxWidthDegrees_ = degrees; }

    void setHiCut(float hz)
    {
        auto coefficient = onePoleLowpassCoefficient(hz, sampleRate_);
        hiCutLeft_.setCoefficient(coefficient);
        hiCutRight_.setCoefficient(coefficient);
    }

    void setFxAdjustDb(float db) { fxAdjustGain_ = std::pow(10.0f, db / 20.0f); }

    void setMix(float wet) { mix_ = clamp01(wet); }

    void reset()
    {
        reverb_.reset();
        delay_.reset();
        detector_.reset();
        shifter_.reset();
        trackingFilter_.reset();
        hiCutLeft_.reset();
        hiCutRight_.reset();
        smoothedSemitoneFromTuning_ = 0.0f;
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

        auto leftIn = left * inLevelLeft_;
        auto rightIn = right * inLevelRight_;
        auto mono = 0.5f * (leftIn + rightIn);

        delay_.write(mono);
        auto delayed = delay_.readLinear(delaySamples_);

        detector_.process(mono);
        if (detector_.hasPitch() && tracking_ != Tracking::kHold)
        {
            auto rawSemitoneFromTuning = 12.0f * std::log2(detector_.frequencyHz() / tuningHz_);
            smoothedSemitoneFromTuning_ = trackingFilter_.process(rawSemitoneFromTuning);
        }

        auto nearestSemitone = std::round(smoothedSemitoneFromTuning_);
        auto correctionCents = (nearestSemitone - smoothedSemitoneFromTuning_) * 100.0f * correctionAmount_;
        auto totalCents = correctionCents + shiftCents_ + static_cast<float>(shiftSemitones_) * 100.0f;
        shifter_.setSemitones(totalCents / 100.0f);
        auto corrected = shifter_.process(delayed);

        auto reverbLeft = corrected;
        auto reverbRight = corrected;
        reverb_.processSample(reverbLeft, reverbRight);

        auto fxLeft = lerp(corrected, reverbLeft, fxMix_);
        auto fxRight = lerp(corrected, reverbRight, fxMix_);

        rotateStereoWidth(fxLeft, fxRight, fxWidthDegrees_);

        fxLeft = hiCutLeft_.process(fxLeft);
        fxRight = hiCutRight_.process(fxRight);

        fxLeft *= fxAdjustGain_;
        fxRight *= fxAdjustGain_;

        left = lerp(dryLeft, fxLeft, mix_);
        right = lerp(dryRight, fxRight, mix_);
    }

  private:
    float sampleRate_ = 48000.0f;

    dsp::algorithms::Chamber reverb_;
    DelayLine delay_;
    PitchDetector detector_;
    PitchShifter shifter_;
    OnePoleLowpass trackingFilter_;

    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
    float delaySamples_ = 0.0f;
    float tuningHz_ = 440.0f;
    float correctionAmount_ = 1.0f;
    Tracking tracking_ = Tracking::kFastest;
    float shiftCents_ = 0.0f;
    int shiftSemitones_ = 0;
    float smoothedSemitoneFromTuning_ = 0.0f;

    float fxMix_ = 0.0f;
    float fxWidthDegrees_ = 0.0f;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
