#pragma once

#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/StereoRotate.h"
#include "dsp/Submixer.h"
#include "dsp/algorithms/Chamber.h"
#include "dsp/algorithms/StereoShift.h"

#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Lexicon PCM81 "Stereo-Chmb" algorithm: a Submixer routing a fixed
 * Chamber reverb against a "Stereo Shifter" FX block, in place of the
 * Dual-Chmb/Dual-Plt/Dual-Inv trio's independent-per-voice "Dual
 * Shifter." Per the manual: "The Stereo-Chmb algorithm is optimized for
 * the best possible shifted audio quality while maintaining the stereo
 * imagery of the source material... This is a true stereo pitch shifter
 * which maintains the stereo image of source material" - one shift
 * amount applied sample-synchronously to both channels, not two
 * independent voices.
 *
 * That's mechanistically exactly `dsp::algorithms::StereoShift`, already
 * built for the Eventide H3000's own Algorithm 103 (see
 * docs/eventide-stereo-shift.md: "one shared Coarse/Fine/Delay/Feedback/
 * Mix value drives both [channels]... each channel's Feedback still
 * returns into its own input, matching the manual's block diagram's two
 * separate feedback triangles - the *value* is shared, not the signal
 * path") - reused unchanged here as the FX block, the first Block shared
 * across both device families' Pitch-shift-class algorithms alongside
 * `householderMix()`'s existing cross-family reuse in the reverb tanks.
 * `StereoShift`'s own internal Mix is fixed to fully wet in prepare();
 * this Graph's own fxMix_ (matching Dual-Chmb's pattern) does the dry/
 * wet blend instead, so the block-level Mix behaves identically across
 * every Dual-FX Pitch algorithm regardless of which FX block it wraps.
 */
class StereoChmbAlgorithm
{
  public:
    enum class Routing
    {
        kParallel,
        kRvbIntoFx,
        kFxIntoRvb,
    };

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Chamber::requiredWorkingBufferSize() +
               dsp::algorithms::StereoShift::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = dsp::algorithms::Chamber::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        auto shifterSize = dsp::algorithms::StereoShift::requiredWorkingBufferSize();
        shifter_.prepare(sampleRate, workingBuffer.subspan(offset, shifterSize));
        offset += shifterSize;

        reverb_.setMix(1.0f);
        shifter_.setMix(1.0f);
        shifter_.setFeedback(0.0f);

        setSends(dsp::Submixer::Sends::kStereo);
        setReturns(dsp::Submixer::Returns::kStereo);
        setRouting(Routing::kParallel);
        setRvbInLevel(1.0f);
        setFxInLevel(1.0f);
        setRvbMix(1.0f);
        setFxMix(1.0f);
        setShiftCents(700.0f);
        setShiftDelaySeconds(0.02f);
        setSpliceSeconds(0.004f);
        setFxWidth(0.0f);
        setHiCut(18000.0f);
        setFxAdjustDb(0.0f);
        setMix(1.0f);
        reset();
    }

    // -- Reverb Block pass-throughs (Chamber) --
    void setDecaySeconds(float seconds) { reverb_.setDecaySeconds(seconds); }
    void setLowRatio(float ratio) { reverb_.setLowRatio(ratio); }
    void setCrossoverFrequency(float hz) { reverb_.setCrossoverFrequency(hz); }
    void setDamping(float amount) { reverb_.setDamping(amount); }
    void setDiffusion(float amount) { reverb_.setDiffusion(amount); }
    void setSize(float sizeNormalized) { reverb_.setSize(sizeNormalized); }
    void setLink(bool linked) { reverb_.setLink(linked); }
    void setShape(float amount) { reverb_.setShape(amount); }
    void setSpread(float amount) { reverb_.setSpread(amount); }
    void setRvbIn(float level) { reverb_.setRvbIn(level); }
    void setRvbOut(float level) { reverb_.setRvbOut(level); }
    void setPreDelaySeconds(float seconds) { reverb_.setPreDelaySeconds(seconds); }
    void setEarlyReflectionLevel(float left, float right) { reverb_.setEarlyReflectionLevel(left, right); }
    void setEarlyReflectionDelaySeconds(float left, float right)
    {
        reverb_.setEarlyReflectionDelaySeconds(left, right);
    }
    void setSpin(float amount) { reverb_.setSpin(amount); }
    void setEkoFeedback(float left, float right) { reverb_.setEkoFeedback(left, right); }
    void setEkoDelaySeconds(float left, float right) { reverb_.setEkoDelaySeconds(left, right); }

    // -- Submixer --
    void setSends(dsp::Submixer::Sends sends) { submixer_.setSends(sends); }
    void setReturns(dsp::Submixer::Returns returns) { submixer_.setReturns(returns); }
    void setRouting(Routing routing) { routing_ = routing; }

    // -- Per-block input trim / dry-wet mix --
    void setRvbInLevel(float level) { rvbInLevel_ = level; }
    void setFxInLevel(float level) { fxInLevel_ = level; }
    void setRvbMix(float mix) { rvbMix_ = clamp01(mix); }
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    // -- Stereo Shifter (one shared shift amount, sample-synchronous L/R) --
    // cents +-3600 (+-3 octaves per the manual's Pitch row).
    void setShiftCents(float cents) { shifter_.setCents(cents); }
    // 0..0.5s, shared by both channels (see StereoShift::setDelaySeconds).
    void setShiftDelaySeconds(float seconds) { shifter_.setDelaySeconds(seconds); }
    // 0..1, shared amount, each channel feeds back into its own input only.
    void setShiftFeedback(float amount) { shifter_.setFeedback(amount); }

    void setSpliceSeconds(float seconds) { shifter_.setGrainSeconds(seconds); }

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
        shifter_.reset();
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
        auto wetLeft = inLeft * fxInLevel_;
        auto wetRight = inRight * fxInLevel_;
        shifter_.processSample(wetLeft, wetRight);
        outLeft = lerp(inLeft, wetLeft, fxMix_);
        outRight = lerp(inRight, wetRight, fxMix_);
    }

    float sampleRate_ = 48000.0f;

    dsp::algorithms::Chamber reverb_;
    dsp::algorithms::StereoShift shifter_;
    dsp::Submixer submixer_;
    Routing routing_ = Routing::kParallel;
    float rvbInLevel_ = 1.0f;
    float fxInLevel_ = 1.0f;
    float rvbMix_ = 1.0f;
    float fxMix_ = 1.0f;

    float fxWidthDegrees_ = 0.0f;
    OnePoleLowpass hiCutLeft_;
    OnePoleLowpass hiCutRight_;
    float fxAdjustGain_ = 1.0f;
    float mix_ = 1.0f;
};
}
