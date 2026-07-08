#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Diffuser.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/StateVariableFilter.h"
#include "dsp/algorithms/Chamber.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Lexicon PCM81-style "M-Band+Rvb": the third of the manual's five
 * 6-Voice algorithms. Per the manual: "This effect features six
 * separately adjustable voices, each with its own level control, delay
 * time, low and high frequency filters, feedback and pan controls. The
 * multi-band effect is in parallel with a Chamber effect... Note also
 * that, in this particular algorithm, the diffuser is within the
 * feedback paths of the multi-band voices. This allows you to create
 * filtered echoes that grow more diffuse with each repeat."
 *
 * Topology (the key difference from Chorus+Rvb, its parallel sibling):
 * feedback there bypasses the diffuser and writes straight back to the
 * delay bank; here it re-enters *through* the diffuser every pass.
 *
 *   L,R -> [+ feedback] -> leftDiffuser_/rightDiffuser_ (shared Diffusion
 *          control with the Chamber reverb) -> leftBank_/rightBank_ (one
 *          shared 3-tap DelayLine per channel) -> each voice: HiCut (2x
 *          cascaded OnePoleLowpass, ~12dB/oct) -> LoCut (StateVariableFilter
 *          highpass output, ~12dB/oct) -> that filtered signal both (a)
 *          feeds Level/Pan into the output sum and (b) feeds back * Fbk
 *          into the diffuser's own input for the next sample.
 *   Chamber (fixed, in parallel, fed the same raw input) -> FX Mix blends
 *          the six-voice sum against the Chamber output.
 */
class MBandRvb
{
  public:
    static constexpr int kNumVoices = 6;
    static constexpr int kNumDiffusers = 2;
    static constexpr std::array<int, kNumDiffusers> kDiffuserLengths = { 97, 149 };
    // 10.922s @ 48kHz, rounded up - the manual's own M-Band+Rvb Delay Time table.
    static constexpr int kBankCapacitySamples = 524300;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        std::size_t diffuserTotal = 0;
        for (auto length : kDiffuserLengths)
        {
            diffuserTotal += static_cast<std::size_t>(length);
        }
        return Chamber::requiredWorkingBufferSize() + 2 * diffuserTotal +
               2 * static_cast<std::size_t>(kBankCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = Chamber::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        for (int i = 0; i < kNumDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kDiffuserLengths[static_cast<std::size_t>(i)]);
            leftDiffuser_.setStageBuffer(static_cast<std::size_t>(i), workingBuffer.subspan(offset, length));
            offset += length;
            rightDiffuser_.setStageBuffer(static_cast<std::size_t>(i), workingBuffer.subspan(offset, length));
            offset += length;
        }

        leftBank_.setBuffer(workingBuffer.subspan(offset, kBankCapacitySamples));
        offset += kBankCapacitySamples;
        rightBank_.setBuffer(workingBuffer.subspan(offset, kBankCapacitySamples));
        offset += kBankCapacitySamples;

        for (auto& svf : voiceLoCut_)
        {
            svf.prepare(sampleRate);
        }

        reverb_.setMix(1.0f);
        setDiffusion(0.5f);
        for (int i = 0; i < kNumVoices; ++i)
        {
            setVoiceDelay(i, 0.0f);
            setVoiceLevel(i, 0.0f);
            setVoicePan(i, 0.0f);
            setVoiceFeedback(i, 0.0f);
            setVoiceHiCut(i, 20000.0f);
            setVoiceLoCut(i, 20.0f);
        }
        setFxMix(0.5f);
        reset();
    }

    // -- Chamber reverb pass-throughs (fixed, in parallel) --
    void setDecaySeconds(float seconds) { reverb_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { reverb_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    // Shared with the multi-band path's own diffuser chains (both L and R).
    void setDiffusion(float amount)
    {
        reverb_.setDiffusion(amount);
        leftDiffuser_.setDiffusion(amount);
        rightDiffuser_.setDiffusion(amount);
    }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setLink(bool linked) { reverb_.setLink(linked); }
    void setShape(float amount) { reverb_.setShape(amount); }
    void setSpread(float amount) { reverb_.setSpread(amount); }
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
    void setFrozen(bool frozen) { reverb_.setFrozen(frozen); }

    // -- Six voices (Voice 1-3 from the left bank, Voice 4-6 from the right) --
    void setVoiceDelay(int index, float delaySeconds)
    {
        voiceDelaySamples_[static_cast<std::size_t>(index)] =
          std::clamp(delaySeconds * 48000.0f, 0.0f, static_cast<float>(kBankCapacitySamples) - 2.0f);
    }
    void setVoiceLevel(int index, float level) { voiceLevel_[static_cast<std::size_t>(index)] = level; }
    void setVoicePan(int index, float pan)
    {
        voicePanLeft_[static_cast<std::size_t>(index)] = (1.0f - pan) * 0.5f;
        voicePanRight_[static_cast<std::size_t>(index)] = (1.0f + pan) * 0.5f;
    }
    // Own-channel only, re-entering through the diffuser - see class doc.
    void setVoiceFeedback(int index, float feedback)
    {
        voiceFeedback_[static_cast<std::size_t>(index)] = feedback;
    }
    // ~12dB/octave lowpass, 20-20000Hz.
    void setVoiceHiCut(int index, float hz)
    {
        auto coefficient = onePoleLowpassCoefficient(hz, sampleRate_);
        voiceHiCutStage1_[static_cast<std::size_t>(index)].setCoefficient(coefficient);
        voiceHiCutStage2_[static_cast<std::size_t>(index)].setCoefficient(coefficient);
    }
    // ~12dB/octave highpass (StateVariableFilter's own stability ceiling
    // caps this around sampleRate/6, i.e. ~8kHz at 48kHz - well above the
    // range a "cut lows" filter is used at in practice).
    void setVoiceLoCut(int index, float hz)
    {
        voiceLoCut_[static_cast<std::size_t>(index)].setCutoff(hz);
        voiceLoCut_[static_cast<std::size_t>(index)].setQ(0.0f);
    }

    // 0 (six-voice multi-band signal only) .. 1 (fully reverbed).
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    void reset()
    {
        reverb_.reset();
        leftDiffuser_.reset();
        rightDiffuser_.reset();
        leftBank_.reset();
        rightBank_.reset();
        for (auto& op : voiceHiCutStage1_) op.reset();
        for (auto& op : voiceHiCutStage2_) op.reset();
        for (auto& svf : voiceLoCut_) svf.reset();
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
        std::array<float, kNumVoices> voiceFiltered{};
        for (int i = 0; i < 3; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto tap = leftBank_.readLinear(voiceDelaySamples_[idx]);
            auto hi = voiceHiCutStage2_[idx].process(voiceHiCutStage1_[idx].process(tap));
            voiceFiltered[idx] = voiceLoCut_[idx].process(hi).highpass;
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto tap = rightBank_.readLinear(voiceDelaySamples_[idx]);
            auto hi = voiceHiCutStage2_[idx].process(voiceHiCutStage1_[idx].process(tap));
            voiceFiltered[idx] = voiceLoCut_[idx].process(hi).highpass;
        }

        float voicesLeft = 0.0f;
        float voicesRight = 0.0f;
        for (int i = 0; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto scaled = voiceFiltered[idx] * voiceLevel_[idx];
            voicesLeft += scaled * voicePanLeft_[idx];
            voicesRight += scaled * voicePanRight_[idx];
        }

        auto leftDiffuserIn = left;
        auto rightDiffuserIn = right;
        for (int i = 0; i < 3; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            leftDiffuserIn += voiceFiltered[idx] * voiceFeedback_[idx];
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            rightDiffuserIn += voiceFiltered[idx] * voiceFeedback_[idx];
        }
        leftBank_.write(leftDiffuser_.process(leftDiffuserIn));
        rightBank_.write(rightDiffuser_.process(rightDiffuserIn));

        auto reverbLeft = left;
        auto reverbRight = right;
        reverb_.processSample(reverbLeft, reverbRight);

        left = lerp(voicesLeft, reverbLeft, fxMix_);
        right = lerp(voicesRight, reverbRight, fxMix_);
    }

  private:
    float sampleRate_ = 48000.0f;

    Chamber reverb_;
    DiffuserChain<kNumDiffusers> leftDiffuser_;
    DiffuserChain<kNumDiffusers> rightDiffuser_;
    DelayLine leftBank_;
    DelayLine rightBank_;

    std::array<OnePoleLowpass, kNumVoices> voiceHiCutStage1_;
    std::array<OnePoleLowpass, kNumVoices> voiceHiCutStage2_;
    std::array<StateVariableFilter, kNumVoices> voiceLoCut_;
    std::array<float, kNumVoices> voiceDelaySamples_{};
    std::array<float, kNumVoices> voiceLevel_{};
    std::array<float, kNumVoices> voicePanLeft_{};
    std::array<float, kNumVoices> voicePanRight_{};
    std::array<float, kNumVoices> voiceFeedback_{};

    float fxMix_ = 0.5f;
};
}
