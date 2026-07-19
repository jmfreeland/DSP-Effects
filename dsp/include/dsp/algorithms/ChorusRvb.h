#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Diffuser.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/algorithms/Plate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Lexicon PCM81-style "Chorus+Rvb": the second of the manual's five
 * 6-Voice algorithms. Per the manual: "The Chorus effect has six
 * separately adjustable voices - allowing the PCM81 to sound like a rack
 * of six digital delay boxes... The 6-voice chorus is in parallel with a
 * plate algorithm, providing two independent stereo effects" - unlike
 * Glide>Hall (series), Chorus+Rvb's 6-voice effect and its Plate reverb
 * both receive the *same* input and are blended by FX Mix.
 *
 * Topology:
 *   L,R -> chorusDiffuserLeft_/Right_ (shares its Diffusion coefficient
 *          with the Plate reverb's own internal diffusion - "the
 *          Diffusion parameter is shared by both the reverb and the
 *          chorus effect") -> High Cut (row 0's own Controls-row slot,
 *          replacing "Voice Dif" for this algorithm) ->
 *   leftBank_/rightBank_ (one shared 3-tap DelayLine per channel, taps
 *          Voice1-3 from leftBank_, Voice4-6 from rightBank_) -> each
 *          voice's own LFO wobbles its read position by 0..Depth ms
 *          *above* its base Delay Time (same unipolar convention
 *          StringModeller's own chorus voice uses, and for the same
 *          reason: a real PCM81 preset's Depth is frequently as large as
 *          or larger than its Delay Time - e.g. the factory "Prime Blue"
 *          preset pairs a 9ms Voice3 delay with an 18ms depth - so a
 *          symmetric +/-Depth sweep around Delay Time routinely drives
 *          the tap negative, which used to clamp to 0 and freeze the
 *          read position for a third of every cycle: an audible
 *          "wow-wow-wow" glitch every LFO cycle, not smooth chorus) at
 *          Rate Hz - "allowing the PCM81 to sound like a rack of six
 *          digital delay boxes" -> Level/Pan ->
 *          summed, feeding back into its own bank only (own-channel Fbk;
 *          unlike Glide>Hall, the manual's own text for this algorithm
 *          describes no cross-feedback between banks).
 *   Plate (fixed, in parallel, fed the same diffused input) -> FX Mix
 *          blends the chorus voices' sum against the Plate output (0 =
 *          chorus only, 1 = reverb only).
 */
class ChorusRvb
{
  public:
    static constexpr int kNumVoices = 6;
    static constexpr int kNumChorusDiffusers = 2;
    static constexpr std::array<int, kNumChorusDiffusers> kChorusDiffuserLengths = { 97, 149 };
    // 1.365s base range + 0.5s max modulation depth headroom, rounded up.
    static constexpr int kBankCapacitySamples = 89600;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        std::size_t diffuserTotal = 0;
        for (auto length : kChorusDiffuserLengths)
        {
            diffuserTotal += static_cast<std::size_t>(length);
        }
        return Plate::requiredWorkingBufferSize() + 2 * diffuserTotal +
               2 * static_cast<std::size_t>(kBankCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = Plate::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        for (int i = 0; i < kNumChorusDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kChorusDiffuserLengths[static_cast<std::size_t>(i)]);
            chorusDiffuserLeft_.setStageBuffer(static_cast<std::size_t>(i), workingBuffer.subspan(offset, length));
            offset += length;
            chorusDiffuserRight_.setStageBuffer(static_cast<std::size_t>(i), workingBuffer.subspan(offset, length));
            offset += length;
        }

        leftBank_.setBuffer(workingBuffer.subspan(offset, kBankCapacitySamples));
        offset += kBankCapacitySamples;
        rightBank_.setBuffer(workingBuffer.subspan(offset, kBankCapacitySamples));
        offset += kBankCapacitySamples;

        for (auto& lfo : voiceLfo_)
        {
            lfo.setFrequency(0.0f, sampleRate_);
        }

        reverb_.setMix(1.0f);
        setDiffusion(0.5f);
        setChorusHighCut(18000.0f);
        setChorusMaster(100.0f, 100.0f);
        for (int i = 0; i < kNumVoices; ++i)
        {
            setVoiceDelay(i, 0.0f);
            setVoiceLevel(i, 0.0f);
            setVoicePan(i, 0.0f);
            setVoiceFeedback(i, 0.0f);
            setVoiceChorus(i, 0.0f, 0.0f);
        }
        setFxMix(0.5f);
        reset();
    }

    // -- Plate reverb pass-throughs (fixed, in parallel) --
    void setDecaySeconds(float seconds) { reverb_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { reverb_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    // Shared with the chorus path's own diffuser chains (both L and R).
    void setDiffusion(float amount)
    {
        reverb_.setDiffusion(amount);
        chorusDiffuserLeft_.setDiffusion(amount);
        chorusDiffuserRight_.setDiffusion(amount);
    }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setLink(bool linked) { reverb_.setLink(linked); }
    void setAttack(float amount) { reverb_.setAttack(amount); }
    void setRvbOut(float level) { reverb_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { reverb_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        reverb_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setEkoDelaySeconds(float left, float right) { reverb_.setEkoDelaySeconds(left, right); }
    void setEkoFeedback(float left, float right) { reverb_.setEkoFeedback(left, right); }
    void setSpin(float amount) { reverb_.setSpin(amount); }

    // Controls row's own High Cut (this algorithm's replacement for the
    // generic "Voice Dif" slot) - a one-pole lowpass on the chorus path
    // only, ahead of the delay bank.
    void setChorusHighCut(float hz)
    {
        auto coefficient = onePoleLowpassCoefficient(hz, sampleRate_);
        chorusHighCutLeft_.setCoefficient(coefficient);
        chorusHighCutRight_.setCoefficient(coefficient);
    }

    // Master Depth/Rate: 0..200%, proportionally scales every voice's own
    // Depth/Rate without altering the underlying per-voice settings -
    // the same "Master" convention Swept Combs already established.
    void setChorusMaster(float depthPercent, float ratePercent)
    {
        masterDepth_ = depthPercent / 100.0f;
        masterRate_ = ratePercent / 100.0f;
        for (int i = 0; i < kNumVoices; ++i)
        {
            refreshVoiceRate(i);
        }
    }

    // -- Six voices (Voice 1-3 from the left bank, Voice 4-6 from the right) --
    void setVoiceDelay(int index, float delaySeconds)
    {
        voiceDelaySamples_[static_cast<std::size_t>(index)] =
          std::clamp(delaySeconds * 48000.0f, 0.0f, static_cast<float>(kBankCapacitySamples) - 24000.0f - 2.0f);
    }
    void setVoiceLevel(int index, float level) { voiceLevel_[static_cast<std::size_t>(index)] = level; }
    void setVoicePan(int index, float pan)
    {
        voicePanLeft_[static_cast<std::size_t>(index)] = (1.0f - pan) * 0.5f;
        voicePanRight_[static_cast<std::size_t>(index)] = (1.0f + pan) * 0.5f;
    }
    // Own-channel only - Voice 1-3 feed the left bus, Voice 4-6 the right.
    void setVoiceFeedback(int index, float feedback)
    {
        voiceFeedback_[static_cast<std::size_t>(index)] = feedback;
    }
    // depthMs 0..500, rateHz 0 (off) ..3.5.
    void setVoiceChorus(int index, float depthMs, float rateHz)
    {
        auto idx = static_cast<std::size_t>(index);
        voiceDepthSamples_[idx] = depthMs * 0.001f * 48000.0f;
        voiceRateHz_[idx] = rateHz;
        refreshVoiceRate(index);
    }

    // 0 (six-voice chorus only) .. 1 (fully reverbed).
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    void setFrozen(bool frozen) { reverb_.setFrozen(frozen); }

    void reset()
    {
        reverb_.reset();
        chorusDiffuserLeft_.reset();
        chorusDiffuserRight_.reset();
        leftBank_.reset();
        rightBank_.reset();
        chorusHighCutLeft_.reset();
        chorusHighCutRight_.reset();
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
        auto diffusedLeft = chorusDiffuserLeft_.process(left);
        auto diffusedRight = chorusDiffuserRight_.process(right);
        auto chorusInLeft = chorusHighCutLeft_.process(diffusedLeft);
        auto chorusInRight = chorusHighCutRight_.process(diffusedRight);

        std::array<float, kNumVoices> voiceOut{};
        for (int i = 0; i < 3; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto modSamples = (0.5f + 0.5f * voiceLfo_[idx].nextSine()) * voiceDepthSamples_[idx] * masterDepth_;
            auto tap = voiceDelaySamples_[idx] + modSamples;
            voiceOut[idx] = leftBank_.readLinear(tap);
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto modSamples = (0.5f + 0.5f * voiceLfo_[idx].nextSine()) * voiceDepthSamples_[idx] * masterDepth_;
            auto tap = voiceDelaySamples_[idx] + modSamples;
            voiceOut[idx] = rightBank_.readLinear(tap);
        }

        float voicesLeft = 0.0f;
        float voicesRight = 0.0f;
        for (int i = 0; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto scaled = voiceOut[idx] * voiceLevel_[idx];
            voicesLeft += scaled * voicePanLeft_[idx];
            voicesRight += scaled * voicePanRight_[idx];
        }

        auto leftBusWrite = chorusInLeft;
        auto rightBusWrite = chorusInRight;
        for (int i = 0; i < 3; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            leftBusWrite += voiceOut[idx] * voiceFeedback_[idx];
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            rightBusWrite += voiceOut[idx] * voiceFeedback_[idx];
        }
        leftBank_.write(leftBusWrite);
        rightBank_.write(rightBusWrite);

        auto reverbLeft = left;
        auto reverbRight = right;
        reverb_.processSample(reverbLeft, reverbRight);

        left = lerp(voicesLeft, reverbLeft, fxMix_);
        right = lerp(voicesRight, reverbRight, fxMix_);
    }

  private:
    void refreshVoiceRate(int index)
    {
        auto idx = static_cast<std::size_t>(index);
        voiceLfo_[idx].setFrequency(voiceRateHz_[idx] * masterRate_, sampleRate_);
    }

    float sampleRate_ = 48000.0f;

    Plate reverb_;
    DiffuserChain<kNumChorusDiffusers> chorusDiffuserLeft_;
    DiffuserChain<kNumChorusDiffusers> chorusDiffuserRight_;
    OnePoleLowpass chorusHighCutLeft_;
    OnePoleLowpass chorusHighCutRight_;
    DelayLine leftBank_;
    DelayLine rightBank_;

    std::array<LFO, kNumVoices> voiceLfo_;
    std::array<float, kNumVoices> voiceDepthSamples_{};
    std::array<float, kNumVoices> voiceRateHz_{};
    std::array<float, kNumVoices> voiceDelaySamples_{};
    std::array<float, kNumVoices> voiceLevel_{};
    std::array<float, kNumVoices> voicePanLeft_{};
    std::array<float, kNumVoices> voicePanRight_{};
    std::array<float, kNumVoices> voiceFeedback_{};
    float masterDepth_ = 1.0f;
    float masterRate_ = 1.0f;

    float fxMix_ = 0.5f;
};
}
