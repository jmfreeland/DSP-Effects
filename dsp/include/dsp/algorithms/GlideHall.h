#pragma once

#include "dsp/DelayLine.h"
#include "dsp/GlideParameter.h"
#include "dsp/Math.h"
#include "dsp/algorithms/ConcertHall.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Lexicon PCM81-style "Glide>Hall": the first of the manual's five
 * 6-Voice algorithms. Per the manual's own diagram: "A stereo pair of
 * 2-tap gliding delays feeds six individually adjustable delay voices.
 * Each voice has its own level, feedback, delay, cross-feedback, and pan
 * parameters. The output of these delay voices is fed into a Concert
 * Hall reverb algorithm" - i.e. series, not parallel, unlike the other
 * four 6-Voice algorithms (see docs/eventide-h3000-notes.md-style
 * docs/lexicon-pcm81-glide-hall.md for the full source-material mapping).
 *
 * Topology:
 *   L,R -> glideLeft_/glideRight_ (2-tap "A"/"B" delay, GlideParameter'd
 *          delay time so changes glide rather than jump - the manual's
 *          own "pitch modulation, flange" character) -> Gld Lvl ->
 *   leftBank_/rightBank_ (one shared 3-tap DelayLine per channel, taps
 *          Voice1-3 from leftBank_, Voice4-6 from rightBank_, each with
 *          its own Delay/Level/Pan/Feedback/Cross-Feedback - the manual's
 *          generic 6-Voice wiring: "Voices 1-3 are connected to input
 *          audio panned to the left. Voices 4-6 ... panned to the
 *          right") -> ConcertHall reverb (fixed, in series) -> FX Mix
 *          blends the pre-reverb six-voice signal against the
 *          post-reverb signal.
 *
 * Both delay banks are fed by a write bus that sums the incoming glide
 * signal with each voice's own weighted feedback (own-channel Fbk) and
 * the opposite bank's weighted cross-feedback (X-Fbk), exactly as the
 * manual's Feedback/Cross-Feedback row describes for both the glide taps
 * and the six voices; DelayLine's natural read-before-write-per-sample
 * ordering means every read already sees the *previous* sample's bus
 * contents, so no extra one-sample-latency bookkeeping is needed here
 * (unlike the H3000 patch-matrix algorithms, which need it explicitly
 * because their sources are arbitrary user-patched destinations rather
 * than a fixed pair of buses).
 */
class GlideHall
{
  public:
    static constexpr int kNumVoices = 6;
    // 42.0ms @ 48kHz, rounded up - the Glide FX row's own documented range.
    static constexpr int kGlideTapCapacitySamples = 2048;
    // 10.581s @ 48kHz, rounded up - the manual's own Glide>Hall Delay Time table.
    static constexpr int kBankCapacitySamples = 508000;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return ConcertHall::requiredWorkingBufferSize() + 2 * static_cast<std::size_t>(kGlideTapCapacitySamples) +
               2 * static_cast<std::size_t>(kBankCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        auto reverbSize = ConcertHall::requiredWorkingBufferSize();
        reverb_.prepare(sampleRate, workingBuffer.subspan(offset, reverbSize));
        offset += reverbSize;

        glideLeft_.setBuffer(workingBuffer.subspan(offset, kGlideTapCapacitySamples));
        offset += kGlideTapCapacitySamples;
        glideRight_.setBuffer(workingBuffer.subspan(offset, kGlideTapCapacitySamples));
        offset += kGlideTapCapacitySamples;

        leftBank_.setBuffer(workingBuffer.subspan(offset, kBankCapacitySamples));
        offset += kBankCapacitySamples;
        rightBank_.setBuffer(workingBuffer.subspan(offset, kBankCapacitySamples));
        offset += kBankCapacitySamples;

        glideTapADelayLeft_.setSampleRate(sampleRate);
        glideTapBDelayLeft_.setSampleRate(sampleRate);
        glideTapADelayRight_.setSampleRate(sampleRate);
        glideTapBDelayRight_.setSampleRate(sampleRate);
        // Fixed internal glide behavior - the manual's Glide FX row has no
        // GldResp/GldRange controls of its own (unlike the 4-Voice
        // algorithms' post-delay), so a moderate always-on glide is used
        // to produce the "pitch modulation, flange" character the manual
        // describes without inventing an unlisted control.
        for (auto* glide : { &glideTapADelayLeft_, &glideTapBDelayLeft_, &glideTapADelayRight_,
                              &glideTapBDelayRight_ })
        {
            glide->setResponse(35.0f);
            glide->setRangeSeconds(1.0f);
        }

        setGlideLevel(1.0f);
        setGlideTapALeft(0.0f, 0.0f);
        setGlideTapARight(0.0f, 0.0f);
        setGlideTapBLeft(0.0f, 0.0f);
        setGlideTapBRight(0.0f, 0.0f);
        setGlideFeedback(0.0f, 0.0f);
        setGlideCrossFeedback(0.0f, 0.0f);
        for (int i = 0; i < kNumVoices; ++i)
        {
            setVoiceDelay(i, 0.0f);
            setVoiceLevel(i, 0.0f);
            setVoicePan(i, 0.0f);
            setVoiceFeedback(i, 0.0f);
            setVoiceCrossFeedback(i, 0.0f);
        }
        reverb_.setMix(1.0f);
        setFxMix(0.5f);
        reset();
    }

    // -- Reverb Block pass-throughs (Concert Hall, fixed) --
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

    // -- Glide FX (row 3) --
    // 0..1 master level for the combined A+B glide delay output.
    void setGlideLevel(float level) { glideLevel_ = level; }

    // level -1..1 (magnitude = level, sign = phase), delaySeconds 0..0.042.
    void setGlideTapALeft(float level, float delaySeconds)
    {
        glideTapALevelLeft_ = level;
        glideTapADelayLeft_.setTarget(secondsToSamples(delaySeconds, kGlideTapCapacitySamples));
    }
    void setGlideTapARight(float level, float delaySeconds)
    {
        glideTapALevelRight_ = level;
        glideTapADelayRight_.setTarget(secondsToSamples(delaySeconds, kGlideTapCapacitySamples));
    }
    void setGlideTapBLeft(float level, float delaySeconds)
    {
        glideTapBLevelLeft_ = level;
        glideTapBDelayLeft_.setTarget(secondsToSamples(delaySeconds, kGlideTapCapacitySamples));
    }
    void setGlideTapBRight(float level, float delaySeconds)
    {
        glideTapBLevelRight_ = level;
        glideTapBDelayRight_.setTarget(secondsToSamples(delaySeconds, kGlideTapCapacitySamples));
    }

    // Fbk L/R: -1..1, own-channel glide delay feedback.
    void setGlideFeedback(float left, float right)
    {
        glideFeedbackLeft_ = left;
        glideFeedbackRight_ = right;
    }

    // X-Fbk L/R: -1..1. Left controls left glide output -> right glide
    // input; Right controls right glide output -> left glide input.
    void setGlideCrossFeedback(float left, float right)
    {
        glideCrossFeedbackLeft_ = left;
        glideCrossFeedbackRight_ = right;
    }

    // -- Six voices (Voice 1-3 from the left bank, Voice 4-6 from the right) --
    // index 0..5, delaySeconds 0..10.581.
    void setVoiceDelay(int index, float delaySeconds)
    {
        voiceDelaySamples_[static_cast<std::size_t>(index)] =
          secondsToSamples(delaySeconds, kBankCapacitySamples);
    }
    void setVoiceLevel(int index, float level) { voiceLevel_[static_cast<std::size_t>(index)] = level; }
    void setVoicePan(int index, float pan)
    {
        voicePanLeft_[static_cast<std::size_t>(index)] = (1.0f - pan) * 0.5f;
        voicePanRight_[static_cast<std::size_t>(index)] = (1.0f + pan) * 0.5f;
    }
    // Voice 1-3 Fbk feeds the left bus; Voice 4-6 Fbk feeds the right bus.
    void setVoiceFeedback(int index, float feedback)
    {
        voiceFeedback_[static_cast<std::size_t>(index)] = feedback;
    }
    // Voice 1-3 X-Fbk feeds the right bus; Voice 4-6 X-Fbk feeds the left bus.
    void setVoiceCrossFeedback(int index, float crossFeedback)
    {
        voiceCrossFeedback_[static_cast<std::size_t>(index)] = crossFeedback;
    }

    // 0 (six-voice signal only) .. 1 (fully reverbed).
    void setFxMix(float mix) { fxMix_ = clamp01(mix); }

    void setFrozen(bool frozen) { reverb_.setFrozen(frozen); }

    void reset()
    {
        reverb_.reset();
        glideLeft_.reset();
        glideRight_.reset();
        leftBank_.reset();
        rightBank_.reset();
        glideTapADelayLeft_.reset();
        glideTapBDelayLeft_.reset();
        glideTapADelayRight_.reset();
        glideTapBDelayRight_.reset();
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
        auto tapA_L = glideLeft_.readLinear(glideTapADelayLeft_.next());
        auto tapB_L = glideLeft_.readLinear(glideTapBDelayLeft_.next());
        auto tapA_R = glideRight_.readLinear(glideTapADelayRight_.next());
        auto tapB_R = glideRight_.readLinear(glideTapBDelayRight_.next());

        auto glideOutLeft = tapA_L * glideTapALevelLeft_ + tapB_L * glideTapBLevelLeft_;
        auto glideOutRight = tapA_R * glideTapALevelRight_ + tapB_R * glideTapBLevelRight_;

        auto glideWriteLeft = left + glideOutLeft * glideFeedbackLeft_ + glideOutRight * glideCrossFeedbackRight_;
        auto glideWriteRight = right + glideOutRight * glideFeedbackRight_ + glideOutLeft * glideCrossFeedbackLeft_;
        glideLeft_.write(glideWriteLeft);
        glideRight_.write(glideWriteRight);

        float voicesLeft = 0.0f;
        float voicesRight = 0.0f;
        std::array<float, kNumVoices> voiceOut{};
        for (int i = 0; i < 3; ++i)
        {
            voiceOut[static_cast<std::size_t>(i)] =
              leftBank_.readLinear(voiceDelaySamples_[static_cast<std::size_t>(i)]);
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            voiceOut[static_cast<std::size_t>(i)] =
              rightBank_.readLinear(voiceDelaySamples_[static_cast<std::size_t>(i)]);
        }
        for (int i = 0; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            auto scaled = voiceOut[idx] * voiceLevel_[idx];
            voicesLeft += scaled * voicePanLeft_[idx];
            voicesRight += scaled * voicePanRight_[idx];
        }

        auto leftBusWrite = glideOutLeft * glideLevel_;
        auto rightBusWrite = glideOutRight * glideLevel_;
        for (int i = 0; i < 3; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            leftBusWrite += voiceOut[idx] * voiceFeedback_[idx];
            rightBusWrite += voiceOut[idx] * voiceCrossFeedback_[idx];
        }
        for (int i = 3; i < kNumVoices; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            rightBusWrite += voiceOut[idx] * voiceFeedback_[idx];
            leftBusWrite += voiceOut[idx] * voiceCrossFeedback_[idx];
        }
        leftBank_.write(leftBusWrite);
        rightBank_.write(rightBusWrite);

        auto reverbLeft = voicesLeft;
        auto reverbRight = voicesRight;
        reverb_.processSample(reverbLeft, reverbRight);

        left = lerp(voicesLeft, reverbLeft, fxMix_);
        right = lerp(voicesRight, reverbRight, fxMix_);
    }

  private:
    // Capacities above are sized for 48kHz regardless of the sample rate
    // passed to prepare() - the same simplification ReverbCore's own
    // pre-delay/early-reflection buffers already use.
    static float secondsToSamples(float seconds, int capacitySamples)
    {
        return std::clamp(seconds * 48000.0f, 0.0f, static_cast<float>(capacitySamples - 2));
    }

    float sampleRate_ = 48000.0f;

    ConcertHall reverb_;
    DelayLine glideLeft_;
    DelayLine glideRight_;
    DelayLine leftBank_;
    DelayLine rightBank_;

    GlideParameter glideTapADelayLeft_;
    GlideParameter glideTapBDelayLeft_;
    GlideParameter glideTapADelayRight_;
    GlideParameter glideTapBDelayRight_;
    float glideTapALevelLeft_ = 0.0f;
    float glideTapBLevelLeft_ = 0.0f;
    float glideTapALevelRight_ = 0.0f;
    float glideTapBLevelRight_ = 0.0f;
    float glideLevel_ = 1.0f;
    float glideFeedbackLeft_ = 0.0f;
    float glideFeedbackRight_ = 0.0f;
    float glideCrossFeedbackLeft_ = 0.0f;
    float glideCrossFeedbackRight_ = 0.0f;

    std::array<float, kNumVoices> voiceDelaySamples_{};
    std::array<float, kNumVoices> voiceLevel_{};
    std::array<float, kNumVoices> voicePanLeft_{};
    std::array<float, kNumVoices> voicePanRight_{};
    std::array<float, kNumVoices> voiceFeedback_{};
    std::array<float, kNumVoices> voiceCrossFeedback_{};

    float fxMix_ = 0.5f;
};
}
