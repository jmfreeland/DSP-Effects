#pragma once

#include "dsp/Diffuser.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/StereoRotate.h"
#include "dsp/algorithms/Res2Plate.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The full PCM81 "Res2>Plate" algorithm: the Res2Plate Block (six
 * diatonically pitch-tracked resonator voices feeding a fixed Plate
 * reverb in series) wrapped in the Controls row common to every
 * algorithm - In Lvl/Pan conditioning, a Voice Diffusion stage (the same
 * interpretation call used by GlideHallAlgorithm/Res1PlateAlgorithm for
 * the "Voice Dif" Controls-row parameter every 6-Voice algorithm lists),
 * and a shared FX Width/Hi-Cut/Adjust chain, finished with a top-level
 * dry/wet Mix.
 *
 *   L,R -> InLvl/InPan -> VoiceDiffusion(L), VoiceDiffusion(R) -> Res2Plate
 *          -> FX Width (StereoRotate) -> Hi-Cut (one-pole) -> FX Adjust
 *   output = lerp(dry, that, mix)
 *
 * See docs/lexicon-pcm81-res2-plate.md for the full source-material
 * mapping and docs/lexicon-pcm81-reference.md for the shared parameter
 * definitions this is built from.
 */
class Res2PlateAlgorithm
{
  public:
    static constexpr int kNumVoiceDiffusers = 2;
    static constexpr std::array<int, kNumVoiceDiffusers> kVoiceDiffuserLengths = { 97, 149 };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        std::size_t voiceDiffuserTotal = 0;
        for (auto length : kVoiceDiffuserLengths)
        {
            voiceDiffuserTotal += static_cast<std::size_t>(length);
        }
        return dsp::algorithms::Res2Plate::requiredWorkingBufferSize() + 2 * voiceDiffuserTotal;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto blockSize = dsp::algorithms::Res2Plate::requiredWorkingBufferSize();
        block_.prepare(sampleRate, workingBuffer.subspan(offset, blockSize));
        offset += blockSize;

        for (int i = 0; i < kNumVoiceDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kVoiceDiffuserLengths[i]);
            voiceDiffuserLeft_.setStageBuffer(i, workingBuffer.subspan(offset, length));
            offset += length;
            voiceDiffuserRight_.setStageBuffer(i, workingBuffer.subspan(offset, length));
            offset += length;
        }

        setInLevel(1.0f, 1.0f);
        setInPan(-1.0f, 1.0f);
        setVoiceDiffusion(0.0f);
        setFxWidth(0.0f);
        setHiCut(18000.0f);
        setFxAdjustDb(0.0f);
        setMix(1.0f);
        reset();
    }

    // -- Block pass-throughs --
    void setDecaySeconds(float seconds) { block_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { block_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { block_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { block_.setDamping(amount); }
    void setDiffusion(float amount) { block_.setDiffusion(amount); }
    void setSize(float sizeNormalized) { block_.setSize(sizeNormalized); }
    void setLink(bool linked) { block_.setLink(linked); }
    void setDefinition(float amount) { block_.setDefinition(amount); }
    void setDepth(float amount) { block_.setDepth(amount); }
    void setRvbIn(float level) { block_.setRvbIn(level); }
    void setRvbOut(float level) { block_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { block_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { block_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        block_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setSpin(float amount) { block_.setSpin(amount); }
    void setChorus(float amount) { block_.setChorus(amount); }
    void setAttack(float amount) { block_.setAttack(amount); }
    void setEkoFeedback(float left, float right) { block_.setEkoFeedback(left, right); }
    void setEkoDelaySeconds(float left, float right) { block_.setEkoDelaySeconds(left, right); }
    void setFrozen(bool frozen) { block_.setFrozen(frozen); }

    void setKey(int key) { block_.setKey(key); }
    void setScale(dsp::Scale scale) { block_.setScale(scale); }
    void setTuneCents(float cents) { block_.setTuneCents(cents); }
    void setFrequencyRange(float minHz, float maxHz) { block_.setFrequencyRange(minHz, maxHz); }
    float trackedFrequencyHz() const { return block_.trackedFrequencyHz(); }

    void setVoiceInterval(int index, dsp::HarmonicInterval interval) { block_.setVoiceInterval(index, interval); }
    void setVoiceLevel(int index, float level) { block_.setVoiceLevel(index, level); }
    void setVoicePan(int index, float pan) { block_.setVoicePan(index, pan); }
    void setVoiceDuration(int index, float seconds) { block_.setVoiceDuration(index, seconds); }
    void setVoiceHiCut(int index, float hz) { block_.setVoiceHiCut(index, hz); }

    void setFxMix(float mix) { block_.setFxMix(mix); }

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

    // 0 (no diffusion) .. 1: density of echoes fed into the resonators.
    void setVoiceDiffusion(float amount)
    {
        voiceDiffuserLeft_.setDiffusion(amount);
        voiceDiffuserRight_.setDiffusion(amount);
    }

    // -360..360 degrees on the combined output.
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

    void reset()
    {
        block_.reset();
        voiceDiffuserLeft_.reset();
        voiceDiffuserRight_.reset();
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

        auto fxLeft = voiceDiffuserLeft_.process(effectsLeft);
        auto fxRight = voiceDiffuserRight_.process(effectsRight);

        block_.processSample(fxLeft, fxRight);

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

    dsp::algorithms::Res2Plate block_;
    DiffuserChain<kNumVoiceDiffusers> voiceDiffuserLeft_;
    DiffuserChain<kNumVoiceDiffusers> voiceDiffuserRight_;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;

    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
    float inPanLeft_ = -1.0f;
    float inPanRight_ = 1.0f;
    float fxWidthDegrees_ = 0.0f;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
