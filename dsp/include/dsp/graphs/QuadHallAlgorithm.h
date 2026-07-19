#pragma once

#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/PitchShiftVoice.h"
#include "dsp/StereoRotate.h"
#include "dsp/algorithms/ConcertHall.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Lexicon PCM81 "Quad>Hall" algorithm: a 4-voice pitch shifter in
 * series with the Concert Hall reverb, matching the manual's own block
 * diagram - "the reverb effect is fixed in position following the pitch
 * shifters, with a final Mix control allowing control over the amount of
 * reverb in the processed sound." Voices 1-2 are fed from the Left input,
 * Voices 3-4 from the Right (per the manual's "Voice 1-4" parameter
 * description), each an independent PitchShiftVoice (own Delay/cents,
 * up to 1.250s), with per-voice Fbk recirculating into its own channel's
 * shared input sum and X-Fbk crossing into the opposite channel's, per
 * the diagram's "V1 X-Fbk".."V4 X-Fbk" boxes.
 *
 *   L -> InLvl/InPan -+-> leftSum  -> V1 (Delay/Pitch) -> V1 Lvl/Pan -+
 *                       (+ Fbk1,Fbk2, XFbk3,XFbk4)                    |
 *                                   -> V2 (Delay/Pitch) -> V2 Lvl/Pan -+-- sum --> Rvb In -> Concert Hall -+
 *                                                                                                           |
 *   R -> InLvl/InPan -+-> rightSum -> V3 (Delay/Pitch) -> V3 Lvl/Pan -+                                     |
 *                       (+ Fbk3,Fbk4, XFbk1,XFbk2)                    |                                     |
 *                                   -> V4 (Delay/Pitch) -> V4 Lvl/Pan -+-- sum ------------------------------+
 *                                                                                                             |
 *                                                            FX Mix (dry-shifted vs. reverberated) ----------+
 *                                                            FX Width (StereoRotate) / Hi-Cut / FX Adjust
 *   output = lerp(dry, that, mix)
 *
 * No dedicated Block: unlike the 4-Voice/6-Voice algorithms (which each
 * add reverb-core-level controls), Quad>Hall reuses ConcertHall exactly
 * as built, so - like ConcertHallAlgorithm owning its own Voice array
 * directly - this Graph owns the 4 PitchShiftVoices itself rather than
 * introducing an intermediate Block with nothing new to add.
 *
 * Simplifications vs. the source material: no MstrCents/MstrScale (a
 * shared additive/multiplicative pitch offset across all 4 voices) and no
 * Low Pitch (per-voice low-frequency-content optimization, trading
 * latency for cleaner low-note tracking) - both are global tone-shaping
 * controls on top of the shift itself rather than new signal-flow, and
 * are deferred the same way GldResp/Splice defaults were left fixed
 * elsewhere in this archive.
 */
class QuadHallAlgorithm
{
  public:
    static constexpr int kNumVoices = 4;
    static constexpr float kMaxVoiceDelaySeconds = 1.25f;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ConcertHall::requiredWorkingBufferSize() +
               static_cast<std::size_t>(kNumVoices) * PitchShiftVoice::requiredWorkingBufferSize(kMaxVoiceDelaySeconds);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = dsp::algorithms::ConcertHall::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        auto voiceSize = PitchShiftVoice::requiredWorkingBufferSize(kMaxVoiceDelaySeconds);
        for (int i = 0; i < kNumVoices; ++i)
        {
            voices_[i].prepare(sampleRate, workingBuffer.subspan(offset, voiceSize), kMaxVoiceDelaySeconds);
            offset += voiceSize;
        }

        reverb_.setMix(1.0f);

        setInLevel(1.0f, 1.0f);
        setInPan(-1.0f, 1.0f);
        setVoice(0, 0.02f, 12.0f, 0.5f, -0.6f);
        setVoice(1, 0.03f, -12.0f, 0.5f, -0.3f);
        setVoice(2, 0.02f, 15.0f, 0.5f, 0.3f);
        setVoice(3, 0.03f, -15.0f, 0.5f, 0.6f);
        setVoiceFeedback(0, 0.0f, 0.0f);
        setVoiceFeedback(1, 0.0f, 0.0f);
        setVoiceFeedback(2, 0.0f, 0.0f);
        setVoiceFeedback(3, 0.0f, 0.0f);
        setSpliceSeconds(0.004f);
        setFxMix(0.35f);
        setFxWidth(45.0f);
        setHiCut(18000.0f);
        setFxAdjustDb(0.0f);
        setMix(1.0f);
        reset();
    }

    // -- Reverb Block pass-throughs (Concert Hall, unmodified) --
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

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void setInPan(float left, float right)
    {
        inPanLeft_ = left;
        inPanRight_ = right;
    }

    // -- Voices (1-2 = Left shifts, 3-4 = Right shifts) --
    // delaySeconds 0..1.25, cents +-3600 (+-3 octaves), level 0..1, pan -1..1.
    void setVoice(int index, float delaySeconds, float cents, float level, float pan)
    {
        auto& voice = voices_[index];
        voice.setDelaySeconds(delaySeconds);
        voice.setSemitones(cents / 100.0f);
        voiceLevel_[index] = level;
        voicePanLeft_[index] = (1.0f - pan) * 0.5f;
        voicePanRight_[index] = (1.0f + pan) * 0.5f;
    }

    // feedback/crossFeedback -1..1 (negative = phase inverted). feedback
    // recirculates into this voice's own channel's shared input sum
    // (Voices 1&2 = Left, 3&4 = Right); crossFeedback into the other
    // channel's, per the manual's "V1 X-Fbk".."V4 X-Fbk".
    void setVoiceFeedback(int index, float feedback, float crossFeedback)
    {
        voiceFeedback_[index] = feedback;
        voiceCrossFeedback_[index] = crossFeedback;
    }

    // Shared crossfade time for every voice's pitch-shift splices (the
    // manual's "Splice" parameter - 4ms is its own suggested default).
    void setSpliceSeconds(float seconds)
    {
        for (auto& voice : voices_)
        {
            voice.setGrainSeconds(seconds);
        }
    }

    // 0 (dry shifted signal only) .. 1 (fully reverberated) balance.
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    // -360..360 degrees on the combined (shifted+reverb) signal.
    void setFxWidth(float degrees) { fxWidthDegrees_ = degrees; }

    // Final high-cut on the combined signal, in Hz.
    void setHiCut(float hz)
    {
        auto coefficient = onePoleLowpassCoefficient(hz, sampleRate_);
        hiCutLeft_.setCoefficient(coefficient);
        hiCutRight_.setCoefficient(coefficient);
    }

    // Output trim in dB.
    void setFxAdjustDb(float db) { fxAdjustGain_ = std::pow(10.0f, db / 20.0f); }

    // 0 (fully dry) .. 1 (fully wet) - the top-level Mix control.
    void setMix(float wet) { mix_ = clamp01(wet); }

    void setFrozen(bool frozen) { reverb_.setFrozen(frozen); }

    void reset()
    {
        reverb_.reset();
        for (auto& voice : voices_)
        {
            voice.reset();
        }
        lastVoiceOutput_.fill(0.0f);
        hiCutLeft_.reset();
        hiCutRight_.reset();
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
        auto leftPanL = (1.0f - inPanLeft_) * 0.5f;
        auto leftPanR = (1.0f + inPanLeft_) * 0.5f;
        auto rightPanL = (1.0f - inPanRight_) * 0.5f;
        auto rightPanR = (1.0f + inPanRight_) * 0.5f;
        auto effectsLeft = leftIn * leftPanL + rightIn * rightPanL;
        auto effectsRight = leftIn * leftPanR + rightIn * rightPanR;

        auto leftFeedback = voiceFeedback_[0] * lastVoiceOutput_[0] + voiceFeedback_[1] * lastVoiceOutput_[1] +
                             voiceCrossFeedback_[2] * lastVoiceOutput_[2] +
                             voiceCrossFeedback_[3] * lastVoiceOutput_[3];
        auto rightFeedback = voiceFeedback_[2] * lastVoiceOutput_[2] + voiceFeedback_[3] * lastVoiceOutput_[3] +
                              voiceCrossFeedback_[0] * lastVoiceOutput_[0] +
                              voiceCrossFeedback_[1] * lastVoiceOutput_[1];

        auto leftSum = effectsLeft + leftFeedback;
        auto rightSum = effectsRight + rightFeedback;

        std::array<float, kNumVoices> voiceInput { leftSum, leftSum, rightSum, rightSum };
        float shiftedLeft = 0.0f;
        float shiftedRight = 0.0f;
        for (int i = 0; i < kNumVoices; ++i)
        {
            auto out = voices_[i].process(voiceInput[i]);
            lastVoiceOutput_[i] = out;
            auto levelled = out * voiceLevel_[i];
            shiftedLeft += levelled * voicePanLeft_[i];
            shiftedRight += levelled * voicePanRight_[i];
        }

        auto reverbLeft = shiftedLeft;
        auto reverbRight = shiftedRight;
        reverb_.processSample(reverbLeft, reverbRight);

        auto fxLeft = lerp(shiftedLeft, reverbLeft, fxMix_);
        auto fxRight = lerp(shiftedRight, reverbRight, fxMix_);

        rotatePcm81Width(fxLeft, fxRight, fxWidthDegrees_);

        fxLeft = hiCutLeft_.process(fxLeft);
        fxRight = hiCutRight_.process(fxRight);

        fxLeft *= fxAdjustGain_;
        fxRight *= fxAdjustGain_;

        left = lerp(dryLeft, fxLeft, mix_);
        right = lerp(dryRight, fxRight, mix_);
    }

  private:
    float sampleRate_ = 48000.0f;

    dsp::algorithms::ConcertHall reverb_;
    std::array<PitchShiftVoice, kNumVoices> voices_;
    std::array<float, kNumVoices> voiceLevel_ {};
    std::array<float, kNumVoices> voicePanLeft_ {};
    std::array<float, kNumVoices> voicePanRight_ {};
    std::array<float, kNumVoices> voiceFeedback_ {};
    std::array<float, kNumVoices> voiceCrossFeedback_ {};
    std::array<float, kNumVoices> lastVoiceOutput_ {};

    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
    float inPanLeft_ = -1.0f;
    float inPanRight_ = 1.0f;
    float fxMix_ = 0.35f;
    float fxWidthDegrees_ = 0.0f;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
