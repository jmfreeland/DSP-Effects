#pragma once

#include "dsp/Decay.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
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
 * Lexicon PCM81-style "Res1>Plate": the fourth of the manual's five
 * 6-Voice algorithms, and the first of its "Resonant Chord" pair (see
 * docs/lexicon-pcm81-res1-plate.md; Res2>Plate is its diatonic sibling,
 * docs/lexicon-pcm81-res2-plate.md). Per the manual: "The Resonant Chord
 * effects use impulsive energy at the inputs to excite six resonant
 * voices (notes). The level, pitch, duration, and high-frequency cutoff
 * of the overtones for each voice are separately controllable... The
 * voices resonate to some degree with any input, but the most effective
 * excitation contains all frequencies, like percussion... The output of
 * the resonator is then fed into a stereo plate reverb effect." Res1
 * assigns pitches "chromatically, in a round-robin" from incoming MIDI
 * notes; no consumer in this project implements MIDI input (the same
 * fact that has shaped every other MIDI-driven PCM81/H3000 feature - see
 * e.g. docs/eventide-band-delay.md's Note Mode skip), so this Block
 * substitutes six directly, independently settable Pitch (Hz) controls
 * for that round-robin note assignment, matching Band Delay's own
 * precedent of replacing a MIDI-note destination with a direct
 * frequency.
 *
 * Each of the six voices is a `StringVoice` (built for the Eventide
 * H3000's String Modeller - the same "delay tuned to a pitch, feedback
 * through a damping filter" Karplus-Strong shape this algorithm's
 * resonators need, reused unchanged across device families), continuously
 * excited by the live input rather than a plucked/triggered stimulation
 * signal - this algorithm has no MIDI note-on to pluck from, and the
 * manual's own text describes continuous excitation ("resonate to some
 * degree with any input"). Voices 1-3 are excited by the left input,
 * Voices 4-6 by the right, matching every other 6-Voice algorithm's
 * shared "left panned audio feeds voices 1-3, right feeds 4-6" convention
 * (manual p.3-8). Duration is implemented as an RT60-style decay target
 * (dsp::rt60ToGain(), already used by Reverb Factory/Dense Room) rather
 * than a raw feedback percentage, so a voice's sustain stays a
 * meaningful, pitch-independent time.
 *
 * The manual's own Pitch row beyond plain pitch assignment (Assign,
 * Tuning, Active, Unison) isn't modeled - see "Known simplifications" in
 * docs/lexicon-pcm81-res1-plate.md.
 */
class Res1Plate
{
  public:
    static constexpr int kNumVoices = 6;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr float kMinVoiceHz = 20.0f;
    static constexpr std::size_t kVoiceCapacitySamples =
      static_cast<std::size_t>(kMaxSampleRate / kMinVoiceHz) + 4;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return Plate::requiredWorkingBufferSize() + static_cast<std::size_t>(kNumVoices) * kVoiceCapacitySamples;
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

        for (int i = 0; i < kNumVoices; ++i)
        {
            setVoicePitch(i, 220.0f);
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

    // -- Six resonator voices (Voice 1-3 excited by left input, 4-6 by right) --
    // index 0..5. hz clamped to [kMinVoiceHz, ~Nyquist].
    void setVoicePitch(int index, float hz)
    {
        auto idx = static_cast<std::size_t>(index);
        auto clampedHz = std::clamp(hz, kMinVoiceHz, sampleRate_ * 0.45f);
        voiceHz_[idx] = clampedHz;
        auto delaySamples = std::min(sampleRate_ / clampedHz, static_cast<float>(kVoiceCapacitySamples - 2));
        voices_[idx].setDelaySamples(delaySamples);
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
        auto idx = static_cast<std::size_t>(index);
        voiceDurationSeconds_[idx] = std::max(seconds, 0.01f);
        auto delaySamples = sampleRate_ / voiceHz_[idx];
        voices_[idx].setFeedback(rt60ToGain(delaySamples, sampleRate_, voiceDurationSeconds_[idx]));
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
    float sampleRate_ = 48000.0f;

    Plate reverb_;
    std::array<StringVoice, kNumVoices> voices_;

    std::array<float, kNumVoices> voiceHz_{};
    std::array<float, kNumVoices> voiceLevel_{};
    std::array<float, kNumVoices> voicePanLeft_{};
    std::array<float, kNumVoices> voicePanRight_{};
    std::array<float, kNumVoices> voiceDurationSeconds_{};

    float fxMix_ = 0.5f;
};
}
