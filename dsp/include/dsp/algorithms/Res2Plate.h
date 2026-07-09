#pragma once

#include "dsp/Decay.h"
#include "dsp/DiatonicScale.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/PitchDetector.h"
#include "dsp/StringVoice.h"
#include "dsp/algorithms/Plate.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Lexicon PCM81-style "Res2>Plate": the fifth of the manual's five
 * 6-Voice algorithms, and the diatonic sibling of Res1>Plate (see
 * docs/lexicon-pcm81-res1-plate.md and docs/lexicon-pcm81-res2-plate.md).
 * Shares Res1>Plate's "six resonators excited by the live input, in
 * series with a fixed Plate reverb" topology exactly - the *only*
 * difference the manual states is how resonator pitch gets assigned:
 * "In Res2>Plate, pitches are assigned to the six resonators
 * diatonically - harmonized with the key, scale, and root of your
 * choice. If MIDI note numbers are used to assign pitch, the resonators
 * will constantly be re-tuned to harmonize with the incoming notes."
 *
 * No consumer in this project implements MIDI input, so rather than a
 * MIDI note driving re-harmonization, this Block reuses the exact
 * pitch-tracking mechanism the Eventide H3000's Diatonic Shift already
 * built for the same underlying problem ("the H3000 tracks your pitch
 * and plays the correct notes" - see dsp/algorithms/DiatonicShift.h and
 * docs/eventide-diatonic-shift.md): a `PitchDetector` tracks the mono
 * sum of the input, and each of the six resonators is retuned to a
 * `dsp::HarmonicInterval` relative to that tracked note, computed in
 * scale-degree space exactly as Diatonic Shift's own `voiceShiftSemitones()`
 * does (a "third up" is 4 semitones from the root but only 3 from the
 * 2nd degree, same as real diatonic harmony). This makes Res2>Plate a
 * genuine diatonic harmonizer, not a fixed chromatic voicing - the whole
 * point of the algorithm.
 *
 * Voices 1-3 are excited by the left input, Voices 4-6 by the right
 * (the shared 6-Voice convention, manual p.3-8), and the *tracker* also
 * reads the mono sum of both, so a note played on either channel
 * re-harmonizes every voice together.
 */
class Res2Plate
{
  public:
    static constexpr int kNumVoices = 6;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr float kMinVoiceHz = 20.0f;
    static constexpr std::size_t kVoiceCapacitySamples =
      static_cast<std::size_t>(kMaxSampleRate / kMinVoiceHz) + 4;
    static constexpr std::size_t kDetectorHistorySamples = 8192; // matches DiatonicShift's own sizing

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return Plate::requiredWorkingBufferSize() +
               static_cast<std::size_t>(kNumVoices) * kVoiceCapacitySamples + kDetectorHistorySamples;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = Plate::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        for (int i = 0; i < kNumVoices; ++i)
        {
            voices_[static_cast<std::size_t>(i)].setBuffer(workingBuffer.subspan(offset, kVoiceCapacitySamples));
            offset += kVoiceCapacitySamples;
        }

        detector_.setBuffer(workingBuffer.subspan(offset, kDetectorHistorySamples));
        offset += kDetectorHistorySamples;
        detector_.prepare(sampleRate);
        detector_.setFrequencyRange(80.0f, 800.0f);

        setKey(0);
        setScale(Scale::kMajor);
        setTuneCents(0.0f);
        static constexpr HarmonicInterval kDefaultIntervals[kNumVoices] = {
            HarmonicInterval::kThirdUp,  HarmonicInterval::kFifthUp,   HarmonicInterval::kOctaveUp,
            HarmonicInterval::kSecondUp, HarmonicInterval::kSixthUp,   HarmonicInterval::kSeventhUp,
        };
        for (int i = 0; i < kNumVoices; ++i)
        {
            setVoiceInterval(i, kDefaultIntervals[static_cast<std::size_t>(i)]);
            setVoiceLevel(i, 0.0f);
            setVoicePan(i, 0.0f);
            setVoiceDuration(i, 1.0f);
            setVoiceHiCut(i, 8000.0f);
        }
        reverb_.setMix(1.0f);
        setFxMix(0.5f);
        reset();
    }

    // -- Reverb Block pass-throughs (Plate, fixed) --
    void setDecaySeconds(float seconds) { reverb_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { reverb_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    void setDiffusion(float amount) { reverb_.setDiffusion(amount); }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setLink(bool linked) { reverb_.setLink(linked); }
    void setDefinition(float amount) { reverb_.setDefinition(amount); }
    void setDepth(float amount) { reverb_.setDepth(amount); }
    void setRvbIn(float level) { reverb_.setRvbIn(level); }
    void setRvbOut(float level) { reverb_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { reverb_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        reverb_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setSpin(float amount) { reverb_.setSpin(amount); }
    void setChorus(float amount) { reverb_.setChorus(amount); }
    void setAttack(float amount) { reverb_.setAttack(amount); }
    void setEkoFeedback(float left, float right) { reverb_.setEkoFeedback(left, right); }
    void setEkoDelaySeconds(float left, float right) { reverb_.setEkoDelaySeconds(left, right); }
    void setFrozen(bool frozen) { reverb_.setFrozen(frozen); }

    // -- Diatonic pitch tracking --
    // 0 (C) .. 11 (B): the key signature the tracked note is interpreted against.
    void setKey(int key) { key_ = key; }
    void setScale(Scale scale) { scale_ = scale; }
    // -50..+50 cents: reference tuning offset (matches the H3000 Diatonic
    // Shift's own "Tune" control).
    void setTuneCents(float cents) { tuneCents_ = std::clamp(cents, -50.0f, 50.0f); }
    // Search range for the pitch tracker, in Hz.
    void setFrequencyRange(float minHz, float maxHz) { detector_.setFrequencyRange(minHz, maxHz); }
    // The most recently tracked input frequency, in Hz (0 if none yet) -
    // exposed for UI/metering use.
    float trackedFrequencyHz() const { return detector_.frequencyHz(); }

    // -- Six resonator voices (Voice 1-3 excited by left input, 4-6 by right) --
    // index 0..5: the harmonic interval this voice resonates at, relative
    // to the tracked note.
    void setVoiceInterval(int index, HarmonicInterval interval)
    {
        voiceInterval_[static_cast<std::size_t>(index)] = interval;
    }
    void setVoiceLevel(int index, float level) { voiceLevel_[static_cast<std::size_t>(index)] = level; }
    void setVoicePan(int index, float pan)
    {
        auto idx = static_cast<std::size_t>(index);
        voicePanLeft_[idx] = (1.0f - pan) * 0.5f;
        voicePanRight_[idx] = (1.0f + pan) * 0.5f;
    }
    // RT60-style sustain time, seconds - the manual's "Duration".
    void setVoiceDuration(int index, float seconds)
    {
        voiceDurationSeconds_[static_cast<std::size_t>(index)] = std::max(seconds, 0.01f);
    }
    // Damping lowpass on each voice's own overtones, Hz - the manual's
    // per-voice "high-frequency cutoff of the overtones".
    void setVoiceHiCut(int index, float hz)
    {
        voices_[static_cast<std::size_t>(index)].setDampingCoefficient(onePoleLowpassCoefficient(hz, sampleRate_));
    }

    // 0 (six-voice signal only) .. 1 (fully reverbed).
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    void reset()
    {
        reverb_.reset();
        detector_.reset();
        for (auto& voice : voices_)
        {
            voice.reset();
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
        detector_.process(0.5f * (left + right));
        updateVoicePitches();

        float voicesLeft = 0.0f;
        float voicesRight = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto out = voices_[idx].process(left) * voiceLevel_[idx];
            voicesLeft += out * voicePanLeft_[idx];
            voicesRight += out * voicePanRight_[idx];
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto out = voices_[idx].process(right) * voiceLevel_[idx];
            voicesLeft += out * voicePanLeft_[idx];
            voicesRight += out * voicePanRight_[idx];
        }

        auto reverbLeft = voicesLeft;
        auto reverbRight = voicesRight;
        reverb_.processSample(reverbLeft, reverbRight);

        left = lerp(voicesLeft, reverbLeft, fxMix_);
        right = lerp(voicesRight, reverbRight, fxMix_);
    }

  private:
    void updateVoicePitches()
    {
        if (!detector_.hasPitch())
        {
            return; // hold the last-tuned pitches through silence/unvoiced stretches
        }

        auto trackedHz = detector_.frequencyHz();
        auto tunedReferenceHz = 440.0f * std::pow(2.0f, tuneCents_ / 1200.0f);
        auto semitoneFromA4 = 12.0f * std::log2(trackedHz / tunedReferenceHz);
        auto semitoneFromC = semitoneFromA4 + 9.0f; // A is 9 semitones above C
        auto semitoneFromTonic = semitoneFromC - static_cast<float>(key_);
        auto detectedDegree = nearestScaleDegreeIndex(scale_, semitoneFromTonic);

        for (int i = 0; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto shiftSemitones = voiceShiftSemitones(voiceInterval_[idx], detectedDegree, semitoneFromTonic);
            auto targetHz =
              std::clamp(trackedHz * std::pow(2.0f, shiftSemitones / 12.0f), kMinVoiceHz, sampleRate_ * 0.45f);
            auto delaySamples = std::min(sampleRate_ / targetHz, static_cast<float>(kVoiceCapacitySamples - 2));
            voices_[idx].setDelaySamples(delaySamples);
            voices_[idx].setFeedback(rt60ToGain(delaySamples, sampleRate_, voiceDurationSeconds_[idx]));
        }
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

    float sampleRate_ = 48000.0f;

    Plate reverb_;
    std::array<StringVoice, kNumVoices> voices_;
    PitchDetector detector_;

    int key_ = 0;
    Scale scale_ = Scale::kMajor;
    float tuneCents_ = 0.0f;

    std::array<HarmonicInterval, kNumVoices> voiceInterval_{};
    std::array<float, kNumVoices> voiceLevel_{};
    std::array<float, kNumVoices> voicePanLeft_{};
    std::array<float, kNumVoices> voicePanRight_{};
    std::array<float, kNumVoices> voiceDurationSeconds_{};

    float fxMix_ = 0.5f;
};
}
