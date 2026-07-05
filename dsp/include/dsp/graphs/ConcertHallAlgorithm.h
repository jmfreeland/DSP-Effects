#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/Voice.h"
#include "dsp/algorithms/ConcertHall.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The full PCM81 "Concert Hall" algorithm: the Concert Hall reverb Block
 * wrapped in the 4-Voice "Reverb Shell" the manual describes as common to
 * every 4-Voice algorithm - 4 independent delay Voices (Level/Delay/
 * Feedback/Pan) running in parallel with the reverb, 2 post-delay taps
 * off the reverb's own output, and a shared FX Mix/Width/Hi-Cut/Adjust
 * chain, finished with a top-level dry/wet Mix.
 *
 *   dry -+-> ConcertHall (forced fully wet)  -+-> postDelay L/R -+
 *        |                                    |                 |
 *        +-> Voice 1..4 (parallel)  ----------+-- FX Mix --------+-- postDelayMix
 *                                                                 |
 *                                              FX Width (mid/side)
 *                                              Hi-Cut (one-pole)
 *                                              FX Adjust (output gain)
 *   output = lerp(dry, that, mix)
 *
 * Simplifications vs. the source material: no separate "Voice
 * Diffusion" stage ahead of the voices, no independent Rvb Width vs FX
 * Width, and FX Width is a plain mid/side scale rather than the PCM81's
 * full mono/stereo/surround/phase-invert table. See
 * docs/lexicon-pcm81-reference.md for what a Reverb Shell "should" have.
 */
class ConcertHallAlgorithm
{
  public:
    static constexpr int kNumVoices = 4;
    // 1.365s @ 48kHz - the 4-Voice algorithms' documented max delay/post-delay range.
    static constexpr int kVoiceCapacitySamples = 65520;
    static constexpr int kPostDelayCapacitySamples = 65520;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ConcertHall::requiredWorkingBufferSize() +
               static_cast<std::size_t>(kNumVoices) * kVoiceCapacitySamples +
               2 * static_cast<std::size_t>(kPostDelayCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = dsp::algorithms::ConcertHall::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        for (int i = 0; i < kNumVoices; ++i)
        {
            voices_[i].setBuffer(workingBuffer.subspan(offset, kVoiceCapacitySamples));
            offset += kVoiceCapacitySamples;
        }

        postDelayLeft_.setBuffer(workingBuffer.subspan(offset, kPostDelayCapacitySamples));
        offset += kPostDelayCapacitySamples;
        postDelayRight_.setBuffer(workingBuffer.subspan(offset, kPostDelayCapacitySamples));
        offset += kPostDelayCapacitySamples;

        // The reverb Block always runs fully wet inside the Graph; the
        // Graph applies its own single dry/wet Mix at the very end.
        reverb_.setMix(1.0f);

        setVoice(0, 0.09f, 0.15f, 0.25f, -0.3f);
        setVoice(1, 0.13f, 0.10f, 0.18f, 0.3f);
        setVoice(2, 0.0f, 0.0f, 0.0f, 0.0f);
        setVoice(3, 0.0f, 0.0f, 0.0f, 0.0f);
        setPostDelaySeconds(0.25f, 0.25f);
        setPostDelayMix(0.15f);
        setFxMix(0.75f);
        setFxWidth(1.0f);
        setHiCut(18000.0f);
        setFxAdjustDb(0.0f);
        setMix(1.0f);
        reset();
    }

    // -- Reverb Block pass-throughs --
    void setDecaySeconds(float seconds) { reverb_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { reverb_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    void setDiffusion(float amount) { reverb_.setDiffusion(amount); }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float level) { reverb_.setEarlyReflectionLevel(level); }
    void setSpin(float amount) { reverb_.setSpin(amount); }
    void setChorus(float amount) { reverb_.setChorus(amount); }

    // -- Voices --
    // delaySeconds 0..1.365, feedback/level -1..1 (negative = phase inverted), pan -1..1.
    void setVoice(int index, float delaySeconds, float feedback, float level, float pan)
    {
        auto& voice = voices_[index];
        voice.setDelaySamples(std::clamp(delaySeconds * sampleRate_, 0.0f,
                                          static_cast<float>(kVoiceCapacitySamples - 2)));
        voice.setFeedback(feedback);
        voice.setLevel(level);
        voice.setPan(pan);
    }

    // -- Post-delay (taps off the reverb's own wet output) --
    void setPostDelaySeconds(float leftSeconds, float rightSeconds)
    {
        postDelayLeftSamples_ = std::clamp(leftSeconds * sampleRate_, 0.0f,
                                            static_cast<float>(kPostDelayCapacitySamples - 2));
        postDelayRightSamples_ = std::clamp(rightSeconds * sampleRate_, 0.0f,
                                             static_cast<float>(kPostDelayCapacitySamples - 2));
    }

    // 0 (no post-delay heard) .. 1 (post-delay fully blended in).
    void setPostDelayMix(float mix) { postDelayMix_ = clamp01(mix); }

    // 0 (all Voices) .. 1 (all reverb) balance of the two parallel paths.
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    // 0 (mono) .. 1 (neutral) .. 2 (exaggerated stereo), a mid/side scale.
    void setFxWidth(float width) { fxWidth_ = std::max(width, 0.0f); }

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
        for (auto& v : voices_)
        {
            v.reset();
        }
        postDelayLeft_.reset();
        postDelayRight_.reset();
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
        auto dryMono = 0.5f * (dryLeft + dryRight);

        auto reverbLeft = left;
        auto reverbRight = right;
        reverb_.processSample(reverbLeft, reverbRight);

        float voicesLeft = 0.0f;
        float voicesRight = 0.0f;
        for (auto& voice : voices_)
        {
            auto out = voice.process(dryMono);
            voicesLeft += out.left;
            voicesRight += out.right;
        }

        auto fxLeft = lerp(voicesLeft, reverbLeft, fxMix_);
        auto fxRight = lerp(voicesRight, reverbRight, fxMix_);

        auto postLeft = postDelayLeft_.readLinear(postDelayLeftSamples_);
        postDelayLeft_.write(reverbLeft);
        auto postRight = postDelayRight_.readLinear(postDelayRightSamples_);
        postDelayRight_.write(reverbRight);

        fxLeft = lerp(fxLeft, postLeft, postDelayMix_);
        fxRight = lerp(fxRight, postRight, postDelayMix_);

        auto mid = 0.5f * (fxLeft + fxRight);
        auto side = 0.5f * (fxLeft - fxRight) * fxWidth_;
        fxLeft = mid + side;
        fxRight = mid - side;

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
    std::array<Voice, kNumVoices> voices_;
    DelayLine postDelayLeft_;
    DelayLine postDelayRight_;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;

    float postDelayLeftSamples_ = 0.0f;
    float postDelayRightSamples_ = 0.0f;
    float postDelayMix_ = 0.0f;
    float fxMix_ = 0.75f;
    float fxWidth_ = 1.0f;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
