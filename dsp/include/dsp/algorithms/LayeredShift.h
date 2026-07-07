#pragma once

#include "dsp/Math.h"
#include "dsp/PitchShiftVoice.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Layered Shift" - Algorithm 101, the second
 * H3000 algorithm built in this archive (see docs/eventide-h3000-notes.md
 * and docs/eventide-layered-shift.md). Per the Instruction Manual's own
 * page: "This algorithm uses the left input to create two separate pitch
 * shifted outputs. The range of the shifters is up one octave and down
 * two octaves... The result... instant 3 part harmony." Unlike Diatonic
 * Shift (Algorithm 100), there is no pitch tracking or scale awareness
 * here - each Voice's shift is a direct, fixed Coarse/Fine cents amount,
 * matching the manual's own parameter.
 *
 * Topology, matching the manual's block diagram:
 *
 *   Left In -> sum -+-> L Delay -> Left Shift  -> Left Out
 *                    +-> R Delay -> Right Shift -> Right Out
 *   Left Out  * L Feedback --\
 *   Right Out * R Feedback --+--> back into the shared sum
 *
 * (Right In is not part of the signal path - the manual's own Description
 * says "the left input", not a stereo sum; see docs/eventide-layered-shift.md.)
 */
class LayeredShift
{
  public:
    // Matches Algorithm 101's own documented Delay parameter range.
    static constexpr float kMaxDelaySeconds = 1.0f;

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
        setLeftCents(400.0f);   // a major 3rd up
        setRightCents(700.0f);  // a perfect 5th up
        setLeftFeedback(0.0f);
        setRightFeedback(0.0f);
        setLeftMix(0.5f);
        setRightMix(0.5f);
        reset();
    }

    // 10-300ms, shared by both Voices (see PitchShifter::setGrainSeconds).
    void setGrainSeconds(float seconds)
    {
        leftVoice_.setGrainSeconds(seconds);
        rightVoice_.setGrainSeconds(seconds);
    }

    // 0-1s, independent per Voice.
    void setLeftDelaySeconds(float seconds) { leftVoice_.setDelaySeconds(seconds); }
    void setRightDelaySeconds(float seconds) { rightVoice_.setDelaySeconds(seconds); }

    // -2400..+1200 cents (the manual's own Coarse/Fine range): negative
    // transposes down, positive up, 100 cents = one semitone.
    void setLeftCents(float cents) { leftVoice_.setSemitones(cents / 100.0f); }
    void setRightCents(float cents) { rightVoice_.setSemitones(cents / 100.0f); }

    // 0..1 (clamped below unity for stability): feedback from each Voice's
    // output back into the shared input.
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

        auto input = dryLeft + leftFeedback_ * feedbackLeft_ + rightFeedback_ * feedbackRight_;
        auto shiftedLeft = leftVoice_.process(input);
        auto shiftedRight = rightVoice_.process(input);
        feedbackLeft_ = shiftedLeft;
        feedbackRight_ = shiftedRight;

        left = lerp(dryLeft, shiftedLeft, leftMix_);
        right = lerp(dryLeft, shiftedRight, rightMix_);
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
