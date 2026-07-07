#pragma once

#include "dsp/Math.h"
#include "dsp/PitchShifter.h"
#include "dsp/ReverseBuffer.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Reverse Shift" - Algorithm 104 (see
 * docs/eventide-h3000-notes.md and docs/eventide-reverse-shift.md). Per
 * the manual: "This algorithm speaks, sings or grunts back to you in
 * reverse with pitch shift. Two pitch shifters in fact... The Reverse
 * Pitch Shift is a one-channel-in, two-channels-out algorithm." Unlike
 * every other H3000 pitch-shift algorithm in this archive so far, the
 * shift here isn't a smooth continuous pitch change - it's genuine
 * time-reversed ("tape reverse") playback of a settable-length splice,
 * with an additional pitch shift layered on top:
 *
 *   Left In, Right In -> sum -+-> L Reverse -> Left Shift  -> Left Out
 *                              +-> R Reverse -> Right Shift -> Right Out
 *   Left Out  * L Feedback --\
 *   Right Out * R Feedback --+--> back into the shared sum
 *
 * Each channel gets its own dsp::ReverseBuffer (independent splice
 * Length) feeding its own dsp::PitchShifter (independent Coarse/Fine),
 * matching the manual's "the left length is independent of the right."
 */
class ReverseShift
{
  public:
    // Matches Algorithm 104's own documented Length parameter range.
    static constexpr float kMaxLengthSeconds = 1.4f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kShifterCapacitySamples =
      static_cast<std::size_t>(PitchShifter::kMaxGrainSeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return 2 * (ReverseBuffer::requiredCapacity(kMaxLengthSeconds, kMaxSampleRate) +
                     kShifterCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        auto reverseCapacity = ReverseBuffer::requiredCapacity(kMaxLengthSeconds, kMaxSampleRate);
        std::size_t offset = 0;
        leftReverse_.setBuffer(workingBuffer.subspan(offset, reverseCapacity));
        offset += reverseCapacity;
        leftShifter_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));
        offset += kShifterCapacitySamples;
        rightReverse_.setBuffer(workingBuffer.subspan(offset, reverseCapacity));
        offset += reverseCapacity;
        rightShifter_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));

        leftReverse_.prepare(sampleRate);
        rightReverse_.prepare(sampleRate);
        leftShifter_.prepare(sampleRate);
        rightShifter_.prepare(sampleRate);

        setGrainSeconds(0.07f);
        setLeftLengthSeconds(0.15f);
        setRightLengthSeconds(0.15f);
        setLeftCents(0.0f);
        setRightCents(0.0f);
        setLeftFeedback(0.0f);
        setRightFeedback(0.0f);
        setLeftMix(0.5f);
        setRightMix(0.5f);
        reset();
    }

    // 10-300ms, the pitch-shift stage's own grain (see
    // PitchShifter::setGrainSeconds) - independent of Length below.
    void setGrainSeconds(float seconds)
    {
        leftShifter_.setGrainSeconds(seconds);
        rightShifter_.setGrainSeconds(seconds);
    }

    // 0-1.4s: the reverse splice length, independent per channel.
    void setLeftLengthSeconds(float seconds) { leftReverse_.setLengthSeconds(seconds); }
    void setRightLengthSeconds(float seconds) { rightReverse_.setLengthSeconds(seconds); }

    // -2400..+1200 cents (the manual's own Coarse/Fine range), applied
    // on top of the reversal.
    void setLeftCents(float cents) { leftShifter_.setSemitones(cents / 100.0f); }
    void setRightCents(float cents) { rightShifter_.setSemitones(cents / 100.0f); }

    // 0..1 (clamped below unity for stability): feedback from each
    // channel's output back into the shared input.
    void setLeftFeedback(float amount) { leftFeedback_ = std::clamp(amount, 0.0f, 0.99f); }
    void setRightFeedback(float amount) { rightFeedback_ = std::clamp(amount, 0.0f, 0.99f); }

    // 0 (dry) .. 1 (fully reversed), independent per channel.
    void setLeftMix(float wet) { leftMix_ = clamp01(wet); }
    void setRightMix(float wet) { rightMix_ = clamp01(wet); }

    void reset()
    {
        leftReverse_.reset();
        rightReverse_.reset();
        leftShifter_.reset();
        rightShifter_.reset();
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

        auto input =
          0.5f * (dryLeft + dryRight) + leftFeedback_ * feedbackLeft_ + rightFeedback_ * feedbackRight_;

        auto shiftedLeft = leftShifter_.process(leftReverse_.process(input));
        auto shiftedRight = rightShifter_.process(rightReverse_.process(input));
        feedbackLeft_ = shiftedLeft;
        feedbackRight_ = shiftedRight;

        left = lerp(dryLeft, shiftedLeft, leftMix_);
        right = lerp(dryRight, shiftedRight, rightMix_);
    }

  private:
    ReverseBuffer leftReverse_;
    ReverseBuffer rightReverse_;
    PitchShifter leftShifter_;
    PitchShifter rightShifter_;

    float leftFeedback_ = 0.0f;
    float rightFeedback_ = 0.0f;
    float leftMix_ = 0.5f;
    float rightMix_ = 0.5f;
    float feedbackLeft_ = 0.0f;
    float feedbackRight_ = 0.0f;
};
}
