#pragma once

#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/StereoRotate.h"
#include "dsp/algorithms/ChorusRvb.h"

#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The full PCM81 "Chorus+Rvb" algorithm: the ChorusRvb Block (the
 * manual's own parallel six-voice-chorus/Plate-reverb signal path)
 * wrapped in the Controls row common to every algorithm - In Lvl/Pan
 * conditioning and a shared FX Width/Hi-Cut/Adjust chain, finished with
 * a top-level dry/wet Mix. Unlike Glide>Hall's Controls row, this
 * algorithm's row 0 has no separate Voice Diffusion slot (the manual
 * replaces it with the Block's own Chorus High Cut), so the Graph here
 * is thinner than GlideHallAlgorithm's.
 *
 *   L,R -> InLvl/InPan -> ChorusRvb -> FX Width (StereoRotate) ->
 *          Hi-Cut (one-pole) -> FX Adjust
 *   output = lerp(dry, that, mix)
 *
 * See docs/lexicon-pcm81-chorus-rvb.md for the full source-material
 * mapping and docs/lexicon-pcm81-reference.md for the shared parameter
 * definitions this is built from.
 */
class ChorusRvbAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::ChorusRvb::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        block_.prepare(sampleRate, workingBuffer);

        setInLevel(1.0f, 1.0f);
        setInPan(-1.0f, 1.0f);
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
    void setAttack(float amount) { block_.setAttack(amount); }
    void setRvbOut(float level) { block_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { block_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { block_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        block_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setEkoDelaySeconds(float left, float right) { block_.setEkoDelaySeconds(left, right); }
    void setEkoFeedback(float left, float right) { block_.setEkoFeedback(left, right); }
    void setSpin(float amount) { block_.setSpin(amount); }
    void setFrozen(bool frozen) { block_.setFrozen(frozen); }

    void setChorusHighCut(float hz) { block_.setChorusHighCut(hz); }
    void setChorusMaster(float depthPercent, float ratePercent) { block_.setChorusMaster(depthPercent, ratePercent); }

    void setVoiceDelay(int index, float delaySeconds) { block_.setVoiceDelay(index, delaySeconds); }
    void setVoiceLevel(int index, float level) { block_.setVoiceLevel(index, level); }
    void setVoicePan(int index, float pan) { block_.setVoicePan(index, pan); }
    void setVoiceFeedback(int index, float feedback) { block_.setVoiceFeedback(index, feedback); }
    void setVoiceChorus(int index, float depthMs, float rateHz) { block_.setVoiceChorus(index, depthMs, rateHz); }

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
        auto fxLeft = leftIn * leftPanL + rightIn * rightPanL;
        auto fxRight = leftIn * leftPanR + rightIn * rightPanR;

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

    dsp::algorithms::ChorusRvb block_;
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
