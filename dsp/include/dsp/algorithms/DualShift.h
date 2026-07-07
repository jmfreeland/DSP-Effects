#pragma once

#include "dsp/Math.h"
#include "dsp/PitchShiftVoice.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Dual Shift" - Algorithm 102 (see
 * docs/eventide-h3000-notes.md and docs/eventide-dual-shift.md). Per the
 * manual: "Algorithm 102 gives you two completely separate pitch
 * shifters. One pitch shifter uses the left channel input and output
 * while the other uses the right channel." Unlike Layered Shift
 * (Algorithm 101), there is no shared input or shared feedback point -
 * the two channels never interact:
 *
 *   Left In  -+-> L Delay -> Left Shift  -> Left Out
 *              \--------------------------------/  (* L Feedback)
 *   Right In -+-> R Delay -> Right Shift -> Right Out
 *              \--------------------------------/  (* R Feedback)
 *
 * Each channel's Feedback returns into *its own* input, not the other
 * channel's - a genuinely independent stereo pair, not a mono-summed or
 * cross-feeding topology.
 */
class DualShift
{
  public:
    // Matches Algorithm 102's own documented Delay parameter range
    // (shorter than Layered Shift's 1s).
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
        setLeftDelaySeconds(0.05f);
        setRightDelaySeconds(0.05f);
        setLeftCents(0.0f);
        setRightCents(0.0f);
        setLeftFeedback(0.0f);
        setRightFeedback(0.0f);
        setLeftMix(0.5f);
        setRightMix(0.5f);
        reset();
    }

    // 10-300ms, shared by both channels (see PitchShifter::setGrainSeconds).
    void setGrainSeconds(float seconds)
    {
        leftVoice_.setGrainSeconds(seconds);
        rightVoice_.setGrainSeconds(seconds);
    }

    // 0-500ms, independent per channel.
    void setLeftDelaySeconds(float seconds) { leftVoice_.setDelaySeconds(seconds); }
    void setRightDelaySeconds(float seconds) { rightVoice_.setDelaySeconds(seconds); }

    // -2400..+1200 cents (the manual's own Coarse/Fine range).
    void setLeftCents(float cents) { leftVoice_.setSemitones(cents / 100.0f); }
    void setRightCents(float cents) { rightVoice_.setSemitones(cents / 100.0f); }

    // 0..1 (clamped below unity for stability): feedback from each
    // channel's output back into *that same channel's* input.
    void setLeftFeedback(float amount) { leftFeedback_ = std::clamp(amount, 0.0f, 0.99f); }
    void setRightFeedback(float amount) { rightFeedback_ = std::clamp(amount, 0.0f, 0.99f); }

    // 0 (dry) .. 1 (fully shifted), independent per channel.
    void setLeftMix(float wet) { leftMix_ = clamp01(wet); }
    void setRightMix(float wet) { rightMix_ = clamp01(wet); }

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

        auto shiftedLeft = leftVoice_.process(dryLeft + leftFeedback_ * feedbackLeft_);
        auto shiftedRight = rightVoice_.process(dryRight + rightFeedback_ * feedbackRight_);
        feedbackLeft_ = shiftedLeft;
        feedbackRight_ = shiftedRight;

        left = lerp(dryLeft, shiftedLeft, leftMix_);
        right = lerp(dryRight, shiftedRight, rightMix_);
    }

  private:
    PitchShiftVoice leftVoice_;
    PitchShiftVoice rightVoice_;

    float leftFeedback_ = 0.0f;
    float rightFeedback_ = 0.0f;
    float leftMix_ = 0.5f;
    float rightMix_ = 0.5f;
    float feedbackLeft_ = 0.0f;
    float feedbackRight_ = 0.0f;
};
}
