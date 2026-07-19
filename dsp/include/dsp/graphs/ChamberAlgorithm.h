#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Diffuser.h"
#include "dsp/GlideParameter.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/StereoRotate.h"
#include "dsp/Voice.h"
#include "dsp/algorithms/Chamber.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The full PCM81 "Chamber" algorithm: the Chamber reverb Block (see
 * dsp/algorithms/Chamber.h - Shape+Spread onset envelope + EkoDly/EkoFbk
 * pre-echo on top of the shared ReverbCore) wrapped in the same 4-Voice
 * "Reverb Shell" front end as ConcertHallAlgorithm.h - see that file's
 * topology diagram and doc comment, which apply unchanged here (the
 * manual describes this front end as common to every 4-Voice algorithm,
 * not just Concert Hall).
 *
 * This class is presently a near-duplicate of ConcertHallAlgorithm.h and
 * PlateAlgorithm.h with the reverb Block swapped out and Chamber's own
 * Shape/Spread/Eko controls added - deliberately, rather than templating
 * the Graph on the reverb core type; see PlateAlgorithm.h's doc comment
 * for why. See docs/lexicon-pcm81-chamber.md.
 */
class ChamberAlgorithm
{
  public:
    static constexpr int kNumVoices = 4;
    // 1.365s @ 48kHz - the 4-Voice algorithms' documented max delay/post-delay range.
    static constexpr int kVoiceCapacitySamples = 65520;
    static constexpr int kPostDelayCapacitySamples = 65520;
    static constexpr int kNumVoiceDiffusers = 2;
    static constexpr std::array<int, kNumVoiceDiffusers> kVoiceDiffuserLengths = { 97, 149 };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        std::size_t voiceDiffuserTotal = 0;
        for (auto length : kVoiceDiffuserLengths)
        {
            voiceDiffuserTotal += static_cast<std::size_t>(length);
        }
        return dsp::algorithms::Chamber::requiredWorkingBufferSize() +
               static_cast<std::size_t>(kNumVoices) * kVoiceCapacitySamples +
               2 * static_cast<std::size_t>(kPostDelayCapacitySamples) + voiceDiffuserTotal;
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = dsp::algorithms::Chamber::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        for (int i = 0; i < kNumVoices; ++i)
        {
            voices_[i].setBuffer(workingBuffer.subspan(offset, kVoiceCapacitySamples));
            voices_[i].setSampleRate(sampleRate);
            offset += kVoiceCapacitySamples;
        }
        postDelayGlideLeft_.setSampleRate(sampleRate);
        postDelayGlideRight_.setSampleRate(sampleRate);

        postDelayLeft_.setBuffer(workingBuffer.subspan(offset, kPostDelayCapacitySamples));
        offset += kPostDelayCapacitySamples;
        postDelayRight_.setBuffer(workingBuffer.subspan(offset, kPostDelayCapacitySamples));
        offset += kPostDelayCapacitySamples;

        for (int i = 0; i < kNumVoiceDiffusers; ++i)
        {
            auto length = static_cast<std::size_t>(kVoiceDiffuserLengths[i]);
            voiceDiffuser_.setStageBuffer(i, workingBuffer.subspan(offset, length));
            offset += length;
        }

        // The reverb Block always runs fully wet inside the Graph; the
        // Graph applies its own single dry/wet Mix at the very end.
        reverb_.setMix(1.0f);

        setInLevel(1.0f, 1.0f);
        setInPan(-1.0f, 1.0f);
        setVoiceDiffusion(0.0f);
        setVoiceGlide(50.0f, 0.0f);
        setVoice(0, 0.09f, 0.15f, 0.25f, -0.3f);
        setVoice(1, 0.13f, 0.10f, 0.18f, 0.3f);
        setVoice(2, 0.0f, 0.0f, 0.0f, 0.0f);
        setVoice(3, 0.0f, 0.0f, 0.0f, 0.0f);
        setPostDelayGlide(50.0f, 0.0f);
        setPostDelaySeconds(0.25f, 0.25f);
        setPostDelayMix(0.15f);
        setClear(false);
        setRvbWidth(45.0f);
        setFxMix(0.75f);
        setFxWidth(45.0f);
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
    void setLink(bool linked) { reverb_.setLink(linked); }
    void setDefinition(float amount) { reverb_.setDefinition(amount); }
    void setDepth(float amount) { reverb_.setDepth(amount); }
    void setRvbIn(float level) { reverb_.setRvbIn(level); }
    void setRvbOut(float level) { reverb_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right)
    {
        reverb_.setEarlyReflectionLevel(left, right);
    }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        reverb_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setSpin(float amount) { reverb_.setSpin(amount); }
    void setChorus(float amount) { reverb_.setChorus(amount); }

    // -- Chamber-specific reverb Block pass-throughs --
    // 0 (no swell: onset reads as immediate/flat) .. 1 (pronounced,
    // slower swell on each new onset).
    void setShape(float amount) { reverb_.setShape(amount); }
    // 0 (swell relaxes quickly) .. 1 (swell lingers up to ~3s, read as
    // extended sustain).
    void setSpread(float amount) { reverb_.setSpread(amount); }
    // 0..1 level of each channel's recirculating pre-echo tap.
    void setEkoFeedback(float left, float right) { reverb_.setEkoFeedback(left, right); }
    // Delay of each channel's pre-echo tap, 0..1.2s.
    void setEkoDelaySeconds(float left, float right) { reverb_.setEkoDelaySeconds(left, right); }

    // -- Input conditioning --
    // level -1..1 (magnitude = level, sign = phase) applied to each input
    // channel before it reaches the reverb/voices (not the final dry path).
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    // pan -1(full left)..+1(full right): how much each physical input
    // channel is routed into the effect's left vs right input. Defaults
    // (-1, +1) are an identity pass-through (matches "50L/50R = unmodified
    // stereo imaging" from the source material).
    void setInPan(float left, float right)
    {
        inPanLeft_ = left;
        inPanRight_ = right;
    }

    // 0 (no diffusion) .. 1: density of echoes fed into the 4 Voices,
    // independent of the reverb's own Diffusion.
    void setVoiceDiffusion(float amount) { voiceDiffuser_.setDiffusion(amount); }

    // -- Voices --
    // delaySeconds 0..1.365, feedback/level -1..1 (negative = phase inverted), pan -1..1.
    // Delay-time changes glide per setVoiceGlide() rather than jumping,
    // unless the change exceeds the glide range.
    void setVoice(int index, float delaySeconds, float feedback, float level, float pan)
    {
        auto& voice = voices_[index];
        voice.setDelaySamples(std::clamp(delaySeconds * sampleRate_, 0.0f,
                                          static_cast<float>(kVoiceCapacitySamples - 2)));
        voice.setFeedback(feedback);
        voice.setLevel(level);
        voice.setPan(pan);
    }

    // Shared glide behavior for all 4 voices' delay-time changes.
    // response 0..100 (0 = ~60s glide, 100 = ~5ms glide); rangeSeconds is
    // the max |delta| that still glides (0 = every change jumps instantly).
    void setVoiceGlide(float response, float rangeSeconds)
    {
        for (auto& voice : voices_)
        {
            voice.setGlide(response, rangeSeconds);
        }
    }

    // Instantly flushes the 4 Voice delay lines on the rising edge, and
    // gates their input silent while held - a footswitch-friendly "clear
    // all old audio and start fresh."
    void setClear(bool clear)
    {
        if (clear && !clearActive_)
        {
            for (auto& voice : voices_)
            {
                voice.reset();
            }
        }
        clearActive_ = clear;
    }

    // -- Post-delay (taps off the reverb's own wet output) --
    // Delay-time changes glide per setPostDelayGlide() rather than jumping.
    void setPostDelaySeconds(float leftSeconds, float rightSeconds)
    {
        postDelayGlideLeft_.setTarget(std::clamp(leftSeconds * sampleRate_, 0.0f,
                                                  static_cast<float>(kPostDelayCapacitySamples - 2)));
        postDelayGlideRight_.setTarget(std::clamp(
          rightSeconds * sampleRate_, 0.0f, static_cast<float>(kPostDelayCapacitySamples - 2)));
    }

    // response 0..100; rangeSeconds is the max |delta| that still glides.
    void setPostDelayGlide(float response, float rangeSeconds)
    {
        postDelayGlideLeft_.setResponse(response);
        postDelayGlideLeft_.setRangeSeconds(rangeSeconds);
        postDelayGlideRight_.setResponse(response);
        postDelayGlideRight_.setRangeSeconds(rangeSeconds);
    }

    // 0 (no post-delay heard) .. 1 (post-delay fully blended in).
    void setPostDelayMix(float mix) { postDelayMix_ = clamp01(mix); }

    // -360..360 degrees, scoped to just the reverb's own output (before
    // FX Mix combines it with the Voices path). See dsp/StereoRotate.h.
    void setRvbWidth(float degrees) { rvbWidthDegrees_ = degrees; }

    // 0 (all Voices) .. 1 (all reverb) balance of the two parallel paths.
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    // -360..360 degrees on the combined (Voices+reverb+postDelay) signal.
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
        for (auto& v : voices_)
        {
            v.reset();
        }
        voiceDiffuser_.reset();
        postDelayLeft_.reset();
        postDelayRight_.reset();
        postDelayGlideLeft_.reset();
        postDelayGlideRight_.reset();
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
        auto effectsMono = 0.5f * (effectsLeft + effectsRight);

        auto reverbLeft = effectsLeft;
        auto reverbRight = effectsRight;
        reverb_.processSample(reverbLeft, reverbRight);
        rotatePcm81Width(reverbLeft, reverbRight, rvbWidthDegrees_);

        auto voiceInput = clearActive_ ? 0.0f : voiceDiffuser_.process(effectsMono);
        float voicesLeft = 0.0f;
        float voicesRight = 0.0f;
        for (auto& voice : voices_)
        {
            auto out = voice.process(voiceInput);
            voicesLeft += out.left;
            voicesRight += out.right;
        }

        auto fxLeft = lerp(voicesLeft, reverbLeft, fxMix_);
        auto fxRight = lerp(voicesRight, reverbRight, fxMix_);

        auto postLeft = postDelayLeft_.readLinear(postDelayGlideLeft_.next());
        postDelayLeft_.write(reverbLeft);
        auto postRight = postDelayRight_.readLinear(postDelayGlideRight_.next());
        postDelayRight_.write(reverbRight);

        fxLeft = lerp(fxLeft, postLeft, postDelayMix_);
        fxRight = lerp(fxRight, postRight, postDelayMix_);

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

    dsp::algorithms::Chamber reverb_;
    DiffuserChain<kNumVoiceDiffusers> voiceDiffuser_;
    std::array<Voice, kNumVoices> voices_;
    DelayLine postDelayLeft_;
    DelayLine postDelayRight_;
    GlideParameter postDelayGlideLeft_;
    GlideParameter postDelayGlideRight_;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;

    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
    float inPanLeft_ = -1.0f;
    float inPanRight_ = 1.0f;
    bool clearActive_ = false;
    float postDelayMix_ = 0.0f;
    float rvbWidthDegrees_ = 0.0f;
    float fxMix_ = 0.75f;
    float fxWidthDegrees_ = 0.0f;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
