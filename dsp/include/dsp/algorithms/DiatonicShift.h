#pragma once

#include "dsp/DelayLine.h"
#include "dsp/DiatonicScale.h"
#include "dsp/Math.h"
#include "dsp/PitchDetector.h"
#include "dsp/PitchShifter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Diatonic Shift" - the box's own default
 * factory program (see docs/eventide-h3000-notes.md) and, per the
 * Instruction Manual's own "Algorithm 100" page, a *real-time pitch
 * tracking* harmonizer: "the H3000 tracks your pitch and plays the
 * correct notes." See docs/eventide-diatonic-shift.md for the full
 * comparison against that primary source - this is the second version of
 * this Block, rebuilt after the first (a fixed-interval transposition
 * with no pitch awareness) turned out to be missing the algorithm's
 * actual defining mechanism, not just a minor simplification of it.
 *
 * Topology, matching the manual's block diagram:
 *
 *   Left In, Right In -> sum (mono) -> Delay (0-1s) -+-> Pitch Detector
 *                                                     +-> Left Voice (PitchShifter) -> Left Out
 *                                                     +-> Right Voice (PitchShifter) -> Right Out
 *   Left Out * L Feedback  --\
 *   Right Out * R Feedback --+--> back into the mono sum
 *
 * Both Voice generators shift the *same* delayed mono signal, each by its
 * own harmonic interval (see dsp::HarmonicInterval), computed from the
 * Pitch Detector's currently-tracked note: the interval is applied in
 * *scale-degree* space (dsp::nearestScaleDegreeIndex() finds which scale
 * degree the tracked note is nearest to, dsp::harmonicIntervalDegreeOffset()
 * adds the chosen interval, dsp::diatonicSemitones() converts back to an
 * absolute semitone target) so a "third up" is 4 semitones from the root
 * but only 3 from the 2nd degree, same as real diatonic harmony - the
 * whole reason this Block exists rather than reusing a fixed transposition.
 * Using the *continuous* detected pitch (not the snapped scale degree) as
 * the shift's reference point means the output lands exactly on the
 * quantized target regardless of small input tuning error, as a natural
 * side effect of the math rather than a separate quantizer stage.
 *
 * Known simplifications (see docs/eventide-diatonic-shift.md for the
 * full list): no fully-custom Scale 1/Scale 2 per-chromatic-note tables
 * (arbitrary-cents harmony, not diatonic-degree-based - doesn't fit this
 * model); pedal-tone octave placement (low/high tonic/dominant) is an
 * original reconstruction, since the manual states the feature but not
 * the exact octave convention; no Source (polyphonic/solo) tracking-
 * aggressiveness control - the detector always assumes a monophonic
 * input.
 */
class DiatonicShift
{
  public:
    // Matches Algorithm 100's own documented Delay parameter range.
    static constexpr float kMaxDelaySeconds = 1.0f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kDetectorHistorySamples = 8192;
    static constexpr std::size_t kShifterCapacitySamples =
      static_cast<std::size_t>(PitchShifter::kMaxGrainSeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return kDelayCapacitySamples + kDetectorHistorySamples + 2 * kShifterCapacitySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        std::size_t offset = 0;
        delay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        detector_.setBuffer(workingBuffer.subspan(offset, kDetectorHistorySamples));
        offset += kDetectorHistorySamples;
        shifterLeft_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));
        offset += kShifterCapacitySamples;
        shifterRight_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));

        detector_.prepare(sampleRate);
        detector_.setFrequencyRange(80.0f, 800.0f);
        shifterLeft_.prepare(sampleRate);
        shifterRight_.prepare(sampleRate);

        setGrainSeconds(0.07f);
        setDelaySeconds(0.05f);
        setKey(0);
        setScale(Scale::kMajor);
        setLeftVoice(HarmonicInterval::kThirdUp);
        setRightVoice(HarmonicInterval::kFifthUp);
        setLeftFeedback(0.0f);
        setRightFeedback(0.0f);
        setLeftMix(0.5f);
        setRightMix(0.5f);
        setTuneCents(0.0f);
        reset();
    }

    // 10-300ms, see PitchShifter::setGrainSeconds.
    void setGrainSeconds(float seconds)
    {
        shifterLeft_.setGrainSeconds(seconds);
        shifterRight_.setGrainSeconds(seconds);
    }

    // 0-1s: time between the input and both Voice outputs (the tracker
    // also reads from this delayed signal, not the raw input).
    void setDelaySeconds(float seconds)
    {
        delaySamples_ =
          std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples - 2));
    }

    // 0 (C) .. 11 (B): the key signature the tracked note is interpreted
    // against - determines which absolute pitches count as "in scale".
    void setKey(int key) { key_ = key; }

    void setScale(Scale scale) { scale_ = scale; }

    // The harmonic interval each Voice plays relative to the tracked note.
    void setLeftVoice(HarmonicInterval interval) { leftVoice_ = interval; }
    void setRightVoice(HarmonicInterval interval) { rightVoice_ = interval; }

    // 0..1 (clamped below unity for stability): feedback from each Voice's
    // output back into the shared mono input, for cascading harmony.
    void setLeftFeedback(float amount) { leftFeedback_ = std::clamp(amount, 0.0f, 0.99f); }
    void setRightFeedback(float amount) { rightFeedback_ = std::clamp(amount, 0.0f, 0.99f); }

    // 0 (dry) .. 1 (fully shifted), independent per channel.
    void setLeftMix(float wet) { leftMix_ = clamp01(wet); }
    void setRightMix(float wet) { rightMix_ = clamp01(wet); }

    // -50..+50 cents: reference tuning offset (matches the H3000's own
    // "Tune" control, for aligning A-440 to an external instrument).
    void setTuneCents(float cents) { tuneCents_ = std::clamp(cents, -50.0f, 50.0f); }

    // Search range for the pitch tracker, in Hz - matches the manual's
    // Low Note/High Note "pitch tracking optimization" controls.
    void setFrequencyRange(float minHz, float maxHz) { detector_.setFrequencyRange(minHz, maxHz); }

    // The most recently tracked input frequency, in Hz (0 if no confident
    // pitch is currently detected) - exposed for UI/metering use.
    float trackedFrequencyHz() const { return detector_.frequencyHz(); }

    void reset()
    {
        delay_.reset();
        detector_.reset();
        shifterLeft_.reset();
        shifterRight_.reset();
        feedbackLeft_ = 0.0f;
        feedbackRight_ = 0.0f;
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

        auto monoInput =
          0.5f * (dryLeft + dryRight) + leftFeedback_ * feedbackLeft_ + rightFeedback_ * feedbackRight_;
        delay_.write(monoInput);
        auto delayed = delay_.readLinear(delaySamples_);

        detector_.process(delayed);
        updateVoiceShifts();

        auto shiftedLeft = shifterLeft_.process(delayed);
        auto shiftedRight = shifterRight_.process(delayed);
        feedbackLeft_ = shiftedLeft;
        feedbackRight_ = shiftedRight;

        left = lerp(dryLeft, shiftedLeft, leftMix_);
        right = lerp(dryRight, shiftedRight, rightMix_);
    }

  private:
    void updateVoiceShifts()
    {
        if (!detector_.hasPitch())
        {
            return; // hold the last shift through silence/unvoiced stretches
        }

        auto tunedReferenceHz = 440.0f * std::pow(2.0f, tuneCents_ / 1200.0f);
        auto semitoneFromA4 = 12.0f * std::log2(detector_.frequencyHz() / tunedReferenceHz);
        auto semitoneFromC = semitoneFromA4 + 9.0f; // A is 9 semitones above C
        auto semitoneFromTonic = semitoneFromC - static_cast<float>(key_);

        auto detectedDegree = nearestScaleDegreeIndex(scale_, semitoneFromTonic);

        shifterLeft_.setSemitones(voiceShiftSemitones(leftVoice_, detectedDegree, semitoneFromTonic));
        shifterRight_.setSemitones(voiceShiftSemitones(rightVoice_, detectedDegree, semitoneFromTonic));
    }

    float voiceShiftSemitones(HarmonicInterval voice, int detectedDegree, float semitoneFromTonic) const
    {
        auto octaveIndex = detectedDegree >= 0 ? detectedDegree / 7 : -((-detectedDegree + 6) / 7);
        int targetDegree = 0;
        switch (voice)
        {
            case HarmonicInterval::kLowTonicPedal:
                targetDegree = (octaveIndex - 1) * 7;
                break;
            case HarmonicInterval::kHighTonicPedal:
                targetDegree = octaveIndex * 7;
                break;
            case HarmonicInterval::kLowDominantPedal:
                targetDegree = (octaveIndex - 1) * 7 + 4;
                break;
            case HarmonicInterval::kHighDominantPedal:
                targetDegree = octaveIndex * 7 + 4;
                break;
            default:
                targetDegree = detectedDegree + harmonicIntervalDegreeOffset(voice);
                break;
        }
        auto targetSemitone = diatonicSemitones(scale_, targetDegree);
        return static_cast<float>(targetSemitone) - semitoneFromTonic;
    }

    DelayLine delay_;
    PitchDetector detector_;
    PitchShifter shifterLeft_;
    PitchShifter shifterRight_;

    float sampleRate_ = 48000.0f;
    float delaySamples_ = 0.0f;
    int key_ = 0;
    Scale scale_ = Scale::kMajor;
    HarmonicInterval leftVoice_ = HarmonicInterval::kThirdUp;
    HarmonicInterval rightVoice_ = HarmonicInterval::kFifthUp;
    float leftFeedback_ = 0.0f;
    float rightFeedback_ = 0.0f;
    float leftMix_ = 0.5f;
    float rightMix_ = 0.5f;
    float tuneCents_ = 0.0f;
    float feedbackLeft_ = 0.0f;
    float feedbackRight_ = 0.0f;
};
}
