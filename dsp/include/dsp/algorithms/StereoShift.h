#pragma once

#include "dsp/Math.h"
#include "dsp/PitchShiftVoice.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Stereo Shift" - Algorithm 103 (see
 * docs/eventide-h3000-notes.md and docs/eventide-stereo-shift.md). Per
 * the manual: "The Stereo Pitch Shift algorithm is for operation with
 * true stereo inputs. The unique deglitching takes both input channels
 * into account without mixing the two audio signals... Parameters of
 * both channels adjust together." Unlike Dual Shift (Algorithm 102),
 * whose two channels are fully independent, Stereo Shift's two channels
 * share one set of Coarse/Fine, Delay, Feedback, and Mix values - they
 * process a genuine stereo pair identically rather than as two unrelated
 * mono voices:
 *
 *   Left In  -+-> L Delay -> Left Shift  -> Left Out
 *              \--------------------------------/  (* shared Feedback)
 *   Right In -+-> R Delay -> Right Shift -> Right Out
 *              \--------------------------------/  (* shared Feedback)
 *
 * (One shared Coarse/Fine/Delay/Feedback/Mix value drives both Voices;
 * each channel's Feedback still returns into its own input, matching
 * the manual's block diagram's two separate feedback triangles - the
 * *value* is shared, not the signal path.)
 */
class StereoShift
{
  public:
    // Matches Algorithm 103's own documented Delay parameter range
    // (same as Dual Shift).
    static constexpr float kMaxDelaySeconds = 0.5f;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return 2 * PitchShiftVoice::requiredWorkingBufferSize(kMaxDelaySeconds);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        auto voiceSize = PitchShiftVoice::requiredWorkingBufferSize(kMaxDelaySeconds);
        leftVoice_.prepare(sampleRate, workingBuffer.subspan(0, voiceSize), kMaxDelaySeconds);
        rightVoice_.prepare(sampleRate, workingBuffer.subspan(voiceSize, voiceSize), kMaxDelaySeconds);

        setGrainSeconds(0.07f);
        setDelaySeconds(0.05f);
        setCents(0.0f);
        setFeedback(0.0f);
        setMix(0.5f);
        reset();
    }

    // 10-300ms (see PitchShifter::setGrainSeconds).
    void setGrainSeconds(float seconds)
    {
        leftVoice_.setGrainSeconds(seconds);
        rightVoice_.setGrainSeconds(seconds);
    }

    // 0-500ms, shared by both channels.
    void setDelaySeconds(float seconds)
    {
        leftVoice_.setDelaySeconds(seconds);
        rightVoice_.setDelaySeconds(seconds);
    }

    // -2400..+1200 cents (the manual's own Coarse/Fine range), shared:
    // both channels track together.
    void setCents(float cents)
    {
        leftVoice_.setSemitones(cents / 100.0f);
        rightVoice_.setSemitones(cents / 100.0f);
    }

    // 0..1 (clamped below unity for stability), shared amount - each
    // channel's own output still feeds back into its own input only.
    void setFeedback(float amount) { feedback_ = std::clamp(amount, 0.0f, 0.99f); }

    // 0 (dry) .. 1 (fully shifted), shared by both channels.
    void setMix(float wet) { mix_ = clamp01(wet); }

    void reset()
    {
        leftVoice_.reset();
        rightVoice_.reset();
        feedbackLeft_ = 0.0f;
        feedbackRight_ = 0.0f;
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

        auto shiftedLeft = leftVoice_.process(dryLeft + feedback_ * feedbackLeft_);
        auto shiftedRight = rightVoice_.process(dryRight + feedback_ * feedbackRight_);
        feedbackLeft_ = shiftedLeft;
        feedbackRight_ = shiftedRight;

        left = lerp(dryLeft, shiftedLeft, mix_);
        right = lerp(dryRight, shiftedRight, mix_);
    }

  private:
    PitchShiftVoice leftVoice_;
    PitchShiftVoice rightVoice_;

    float feedback_ = 0.0f;
    float mix_ = 0.5f;
    float feedbackLeft_ = 0.0f;
    float feedbackRight_ = 0.0f;
};
}
