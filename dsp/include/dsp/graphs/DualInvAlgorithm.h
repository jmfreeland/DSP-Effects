#pragma once

#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/PitchShiftVoice.h"
#include "dsp/StereoRotate.h"
#include "dsp/Submixer.h"
#include "dsp/algorithms/Inverse.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Lexicon PCM81 "Dual-Inv" algorithm: the same Submixer + "Dual
 * Shifter" shape as Dual-Chmb/Dual-Plt (see
 * docs/lexicon-pcm81-dual-chmb.md), with Inverse in place of Chamber -
 * so, like InverseAlgorithm.h, this Graph exposes Duration/Low Slope/
 * Mid Slope/Shape instead of Decay/Low Ratio (Inverse's decay isn't
 * RT60-exponential at all - see docs/lexicon-pcm81-inverse.md) and has
 * no EkoDly/EkoFbk pre-echo (the manual scopes that to Plate/Chamber/
 * Infinite only). See dsp/Submixer.h and docs/lexicon-pcm81-reference.md's
 * "The Pitch algorithms" section.
 */
class DualInvAlgorithm
{
  public:
    static constexpr int kNumVoices = 2;
    static constexpr float kMaxVoiceDelaySeconds = 1.25f;

    enum class Routing
    {
        kParallel,
        kRvbIntoFx,
        kFxIntoRvb,
    };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Inverse::requiredWorkingBufferSize() +
               static_cast<std::size_t>(kNumVoices) * PitchShiftVoice::requiredWorkingBufferSize(kMaxVoiceDelaySeconds);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = dsp::algorithms::Inverse::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        auto voiceSize = PitchShiftVoice::requiredWorkingBufferSize(kMaxVoiceDelaySeconds);
        for (int i = 0; i < kNumVoices; ++i)
        {
            voices_[i].prepare(sampleRate, workingBuffer.subspan(offset, voiceSize), kMaxVoiceDelaySeconds);
            offset += voiceSize;
        }

        reverb_.setMix(1.0f);

        setSends(dsp::Submixer::Sends::kStereo);
        setReturns(dsp::Submixer::Returns::kStereo);
        setRouting(Routing::kParallel);
        setRvbInLevel(1.0f);
        setFxInLevel(1.0f);
        setRvbMix(1.0f);
        setFxMix(1.0f);
        setVoice(0, 0.02f, 700.0f, 0.7f, -0.7f);
        setVoice(1, 0.03f, -700.0f, 0.7f, 0.7f);
        setVoiceFeedback(0, 0.0f, 0.0f);
        setVoiceFeedback(1, 0.0f, 0.0f);
        setSpliceSeconds(0.004f);
        setFxWidth(0.0f);
        setHiCut(18000.0f);
        setFxAdjustDb(0.0f);
        setMix(1.0f);
        reset();
    }

    // -- Reverb Block pass-throughs (Inverse) --
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    void setDiffusion(float amount) { reverb_.setDiffusion(amount); }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setDuration(float seconds) { reverb_.setDuration(seconds); }
    void setLowSlope(float slope) { reverb_.setLowSlope(slope); }
    void setMidSlope(float slope) { reverb_.setMidSlope(slope); }
    void setShape(float amount) { reverb_.setShape(amount); }
    void setRvbIn(float level) { reverb_.setRvbIn(level); }
    void setRvbOut(float level) { reverb_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { reverb_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        reverb_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setSpin(float amount) { reverb_.setSpin(amount); }

    // -- Submixer --
    void setSends(dsp::Submixer::Sends sends) { submixer_.setSends(sends); }
    void setReturns(dsp::Submixer::Returns returns) { submixer_.setReturns(returns); }
    void setRouting(Routing routing) { routing_ = routing; }

    // -- Per-block input trim / dry-wet mix --
    void setRvbInLevel(float level) { rvbInLevel_ = level; }
    void setFxInLevel(float level) { fxInLevel_ = level; }
    void setRvbMix(float mix) { rvbMix_ = clamp01(mix); }
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    // -- Dual Shifter voices --
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

    // feedback/crossFeedback -1..1 (negative = phase inverted): feedback
    // recirculates into this voice's own input, crossFeedback into the
    // other voice's, per the manual's "V1/V2 X-Fbk".
    void setVoiceFeedback(int index, float feedback, float crossFeedback)
    {
        voiceFeedback_[index] = feedback;
        voiceCrossFeedback_[index] = crossFeedback;
    }

    void setSpliceSeconds(float seconds)
    {
        for (auto& voice : voices_)
        {
            voice.setGrainSeconds(seconds);
        }
    }

    // -360..360 degrees on the combined (Submixer-returned) signal.
    void setFxWidth(float degrees) { fxWidthDegrees_ = degrees; }

    void setHiCut(float hz)
    {
        auto coefficient = onePoleLowpassCoefficient(hz, sampleRate_);
        hiCutLeft_.setCoefficient(coefficient);
        hiCutRight_.setCoefficient(coefficient);
    }

    void setFxAdjustDb(float db) { fxAdjustGain_ = std::pow(10.0f, db / 20.0f); }

    // 0 (fully dry) .. 1 (fully wet) - the top-level Mix control.
    void setMix(float wet) { mix_ = clamp01(wet); }

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

        auto sent = submixer_.send(left, right);

        float rvbOutLeft = 0.0f;
        float rvbOutRight = 0.0f;
        float fxOutLeft = 0.0f;
        float fxOutRight = 0.0f;

        switch (routing_)
        {
            case Routing::kParallel:
                processRvb(sent.rvbLeft, sent.rvbRight, rvbOutLeft, rvbOutRight);
                processFx(sent.fxLeft, sent.fxRight, fxOutLeft, fxOutRight);
                break;
            case Routing::kRvbIntoFx:
                processRvb(sent.rvbLeft, sent.rvbRight, rvbOutLeft, rvbOutRight);
                processFx(rvbOutLeft, rvbOutRight, fxOutLeft, fxOutRight);
                rvbOutLeft = 0.0f;
                rvbOutRight = 0.0f;
                break;
            case Routing::kFxIntoRvb:
                processFx(sent.fxLeft, sent.fxRight, fxOutLeft, fxOutRight);
                processRvb(fxOutLeft, fxOutRight, rvbOutLeft, rvbOutRight);
                fxOutLeft = 0.0f;
                fxOutRight = 0.0f;
                break;
        }

        auto received = submixer_.receive(rvbOutLeft, rvbOutRight, fxOutLeft, fxOutRight);
        auto wetLeft = received.left;
        auto wetRight = received.right;

        rotateStereoWidth(wetLeft, wetRight, fxWidthDegrees_);

        wetLeft = hiCutLeft_.process(wetLeft);
        wetRight = hiCutRight_.process(wetRight);

        wetLeft *= fxAdjustGain_;
        wetRight *= fxAdjustGain_;

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(dryRight, wetRight, mix_);
    }

  private:
    void processRvb(float inLeft, float inRight, float& outLeft, float& outRight)
    {
        auto wetLeft = inLeft * rvbInLevel_;
        auto wetRight = inRight * rvbInLevel_;
        reverb_.processSample(wetLeft, wetRight);
        outLeft = lerp(inLeft, wetLeft, rvbMix_);
        outRight = lerp(inRight, wetRight, rvbMix_);
    }

    void processFx(float inLeft, float inRight, float& outLeft, float& outRight)
    {
        auto fxInLeft = inLeft * fxInLevel_;
        auto fxInRight = inRight * fxInLevel_;

        auto sum0 = fxInLeft + voiceFeedback_[0] * lastVoiceOutput_[0] +
                    voiceCrossFeedback_[1] * lastVoiceOutput_[1];
        auto sum1 = fxInRight + voiceFeedback_[1] * lastVoiceOutput_[1] +
                    voiceCrossFeedback_[0] * lastVoiceOutput_[0];

        auto out0 = voices_[0].process(sum0);
        auto out1 = voices_[1].process(sum1);
        lastVoiceOutput_[0] = out0;
        lastVoiceOutput_[1] = out1;

        auto levelled0 = out0 * voiceLevel_[0];
        auto levelled1 = out1 * voiceLevel_[1];
        auto wetLeft = levelled0 * voicePanLeft_[0] + levelled1 * voicePanLeft_[1];
        auto wetRight = levelled0 * voicePanRight_[0] + levelled1 * voicePanRight_[1];

        outLeft = lerp(inLeft, wetLeft, fxMix_);
        outRight = lerp(inRight, wetRight, fxMix_);
    }

    float sampleRate_ = 48000.0f;

    dsp::algorithms::Inverse reverb_;
    dsp::Submixer submixer_;
    Routing routing_ = Routing::kParallel;
    float rvbInLevel_ = 1.0f;
    float fxInLevel_ = 1.0f;
    float rvbMix_ = 1.0f;
    float fxMix_ = 1.0f;

    std::array<PitchShiftVoice, kNumVoices> voices_;
    std::array<float, kNumVoices> voiceLevel_ {};
    std::array<float, kNumVoices> voicePanLeft_ {};
    std::array<float, kNumVoices> voicePanRight_ {};
    std::array<float, kNumVoices> voiceFeedback_ {};
    std::array<float, kNumVoices> voiceCrossFeedback_ {};
    std::array<float, kNumVoices> lastVoiceOutput_ {};

    float fxWidthDegrees_ = 0.0f;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
