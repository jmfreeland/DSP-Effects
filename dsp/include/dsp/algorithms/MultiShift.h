#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/PitchShifter.h"
#include "dsp/ReverseBuffer.h"
#include "dsp/StereoRotate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Multi-Shift" (Algorithm 116), per that
 * algorithm's own manual page: "similar to the dual shift program,
 * allowing discrete stereo pitch shifting. In addition to the pitch
 * shifters, a delay tap has been added to each pitch shift channel,
 * giving a total of four outputs. Each of the four outputs can be
 * panned anywhere in the stereo field... a 'patchable' feedback
 * structure has been set up, allowing each pitch shifter to use any two
 * of the four outputs as feedback. Finally, each of the pitch shifters
 * can be set, independently, to 'reverse pitch shift mode.'"
 *
 * Four sources per sample - L Pitch, R Pitch (each a Delay -> PitchShifter
 * chain, or a ReverseBuffer -> PitchShifter chain in Reverse mode,
 * exactly like Reverse Shift/Algorithm 104), L Delay, R Delay (plain dry
 * delay taps, no pitch shift) - each with its own output Level and Pan,
 * summed into the stereo output. Each pitch shifter's own input can
 * additionally be fed back from any two of the four sources
 * (setLeftFeedbackSource1/2, setRightFeedbackSource1/2), scaled by that
 * path's own amount and a master Feedback scale. Every feedback
 * cross-connection reads the *previous* sample's value of its source -
 * the same one-sample-latency technique Patch Factory uses for its own,
 * much larger, patch matrix - so any user-chosen feedback routing (even
 * routing a channel's own output back into itself) stays well-defined
 * without needing to detect cycles.
 */
class MultiShift
{
  public:
    enum class Source
    {
        kLPitch,
        kRPitch,
        kLDelay,
        kRDelay,
        kCount
    };

    static constexpr float kMaxDelaySeconds = 0.7f;
    static constexpr float kMaxSpliceSeconds = 0.7f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kShifterCapacitySamples =
      static_cast<std::size_t>(PitchShifter::kMaxGrainSeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        auto reverseCapacity = ReverseBuffer::requiredCapacity(kMaxSpliceSeconds, kMaxSampleRate);
        // Per channel: pitch delay + reverse-splice buffer + shifter grain
        // buffer + independent dry delay tap.
        return 2 * (kDelayCapacitySamples + reverseCapacity + kShifterCapacitySamples + kDelayCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        auto reverseCapacity = ReverseBuffer::requiredCapacity(kMaxSpliceSeconds, kMaxSampleRate);

        std::size_t offset = 0;
        leftPitchDelay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        leftReverse_.setBuffer(workingBuffer.subspan(offset, reverseCapacity));
        offset += reverseCapacity;
        leftShifter_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));
        offset += kShifterCapacitySamples;
        leftDryDelay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;

        rightPitchDelay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        rightReverse_.setBuffer(workingBuffer.subspan(offset, reverseCapacity));
        offset += reverseCapacity;
        rightShifter_.setBuffer(workingBuffer.subspan(offset, kShifterCapacitySamples));
        offset += kShifterCapacitySamples;
        rightDryDelay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));

        leftReverse_.prepare(sampleRate_);
        rightReverse_.prepare(sampleRate_);
        leftShifter_.prepare(sampleRate_);
        rightShifter_.prepare(sampleRate_);

        setLeftCents(0.0f);
        setRightCents(0.0f);
        setLeftPitchDelaySeconds(0.02f);
        setRightPitchDelaySeconds(0.02f);
        setLeftDelaySeconds(0.1f);
        setRightDelaySeconds(0.1f);
        setLeftSpliceSeconds(0.15f);
        setRightSpliceSeconds(0.15f);
        setLeftXfadeSlow(false);
        setRightXfadeSlow(false);
        setLeftDirection(false);
        setRightDirection(false);
        setMix(1.0f);
        setFeedbackScale(0.0f);
        setImage(0.0f);
        setLPitchLevel(100.0f);
        setRPitchLevel(100.0f);
        setLDelayLevel(0.0f);
        setRDelayLevel(0.0f);
        setLPitchPan(-1.0f);
        setRPitchPan(1.0f);
        setLDelayPan(-1.0f);
        setRDelayPan(1.0f);
        setLeftFeedback1(0.0f, Source::kLPitch);
        setLeftFeedback2(0.0f, Source::kLDelay);
        setRightFeedback1(0.0f, Source::kRPitch);
        setRightFeedback2(0.0f, Source::kRDelay);

        reset();
    }

    // -- Core controls (#0-8) --
    void setLeftCents(float cents) { leftShifter_.setSemitones(std::clamp(cents, -3600.0f, 3600.0f) / 100.0f); }
    void setRightCents(float cents) { rightShifter_.setSemitones(std::clamp(cents, -3600.0f, 3600.0f) / 100.0f); }
    void setLeftPitchDelaySeconds(float seconds)
    {
        leftPitchDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setRightPitchDelaySeconds(float seconds)
    {
        rightPitchDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setLeftDelaySeconds(float seconds)
    {
        leftDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setRightDelaySeconds(float seconds)
    {
        rightDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setMix(float wet) { mix_ = clamp01(wet); }
    // 0..100 per cent: a master scale applied on top of each path's own
    // Feedback 1/2 amount (the manual's own "Feedback" control - its
    // printed unit, milliseconds, doesn't match its own description as a
    // scaling control, so this treats it as the described percentage
    // scale rather than the printed unit - see docs/eventide-multi-shift.md).
    void setFeedbackScale(float percent0to100) { feedbackScale_ = clamp01(percent0to100 / 100.0f); }
    // -1 (normal L<->R) .. +1 (swapped R<->L), reusing rotateStereoWidth.
    void setImage(float amountMinus1to1)
    {
        imageDegrees_ = std::clamp(amountMinus1to1, -1.0f, 1.0f) * 180.0f;
    }

    // -- Expert output controls (#9-16) --
    void setLPitchLevel(float percent) { levels_[static_cast<std::size_t>(Source::kLPitch)] = clampPercent(percent); }
    void setRPitchLevel(float percent) { levels_[static_cast<std::size_t>(Source::kRPitch)] = clampPercent(percent); }
    void setLDelayLevel(float percent) { levels_[static_cast<std::size_t>(Source::kLDelay)] = clampPercent(percent); }
    void setRDelayLevel(float percent) { levels_[static_cast<std::size_t>(Source::kRDelay)] = clampPercent(percent); }
    void setLPitchPan(float pan) { pans_[static_cast<std::size_t>(Source::kLPitch)] = std::clamp(pan, -1.0f, 1.0f); }
    void setRPitchPan(float pan) { pans_[static_cast<std::size_t>(Source::kRPitch)] = std::clamp(pan, -1.0f, 1.0f); }
    void setLDelayPan(float pan) { pans_[static_cast<std::size_t>(Source::kLDelay)] = std::clamp(pan, -1.0f, 1.0f); }
    void setRDelayPan(float pan) { pans_[static_cast<std::size_t>(Source::kRDelay)] = std::clamp(pan, -1.0f, 1.0f); }

    // -- Patching (#17-24) --
    void setLeftFeedback1(float percent, Source source) { leftFb1Amount_ = clampPercent(percent); leftFb1Source_ = source; }
    void setLeftFeedback2(float percent, Source source) { leftFb2Amount_ = clampPercent(percent); leftFb2Source_ = source; }
    void setRightFeedback1(float percent, Source source) { rightFb1Amount_ = clampPercent(percent); rightFb1Source_ = source; }
    void setRightFeedback2(float percent, Source source) { rightFb2Amount_ = clampPercent(percent); rightFb2Source_ = source; }

    // -- Control (#25-32; Deglitch #27/#31 not exposed - see
    // docs/eventide-multi-shift.md's Known simplifications) --
    void setLeftDirection(bool reverse) { leftReverse_.enabled = reverse; }
    void setRightDirection(bool reverse) { rightReverse_.enabled = reverse; }
    // Fast = short grain ("exactly like our old pitch shifters"), Slow =
    // long grain for glitchless micro-pitch shifting.
    void setLeftXfadeSlow(bool slow) { leftShifter_.setGrainSeconds(slow ? 0.15f : 0.03f); }
    void setRightXfadeSlow(bool slow) { rightShifter_.setGrainSeconds(slow ? 0.15f : 0.03f); }
    void setLeftSpliceSeconds(float seconds) { leftReverse_.setLengthSeconds(std::clamp(seconds, 0.001f, kMaxSpliceSeconds)); }
    void setRightSpliceSeconds(float seconds) { rightReverse_.setLengthSeconds(std::clamp(seconds, 0.001f, kMaxSpliceSeconds)); }

    void reset()
    {
        leftPitchDelay_.reset();
        rightPitchDelay_.reset();
        leftReverse_.reset();
        rightReverse_.reset();
        leftShifter_.reset();
        rightShifter_.reset();
        leftDryDelay_.reset();
        rightDryDelay_.reset();
        previous_.fill(0.0f);
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

        auto leftFbSum =
          (leftFb1Amount_ * previous_[static_cast<std::size_t>(leftFb1Source_)] +
           leftFb2Amount_ * previous_[static_cast<std::size_t>(leftFb2Source_)]) *
          feedbackScale_;
        auto rightFbSum =
          (rightFb1Amount_ * previous_[static_cast<std::size_t>(rightFb1Source_)] +
           rightFb2Amount_ * previous_[static_cast<std::size_t>(rightFb2Source_)]) *
          feedbackScale_;

        auto leftPitchIn = dryLeft + leftFbSum;
        auto rightPitchIn = dryRight + rightFbSum;

        float leftPreShift;
        if (leftReverse_.enabled)
        {
            leftPreShift = leftReverse_.process(leftPitchIn);
        }
        else
        {
            leftPitchDelay_.write(leftPitchIn);
            leftPreShift = leftPitchDelay_.readLinear(leftPitchDelaySamples_);
        }
        auto leftPitchOut = leftShifter_.process(leftPreShift);

        float rightPreShift;
        if (rightReverse_.enabled)
        {
            rightPreShift = rightReverse_.process(rightPitchIn);
        }
        else
        {
            rightPitchDelay_.write(rightPitchIn);
            rightPreShift = rightPitchDelay_.readLinear(rightPitchDelaySamples_);
        }
        auto rightPitchOut = rightShifter_.process(rightPreShift);

        leftDryDelay_.write(dryLeft);
        auto leftDelayOut = leftDryDelay_.readLinear(leftDelaySamples_);
        rightDryDelay_.write(dryRight);
        auto rightDelayOut = rightDryDelay_.readLinear(rightDelaySamples_);

        previous_[static_cast<std::size_t>(Source::kLPitch)] = leftPitchOut;
        previous_[static_cast<std::size_t>(Source::kRPitch)] = rightPitchOut;
        previous_[static_cast<std::size_t>(Source::kLDelay)] = leftDelayOut;
        previous_[static_cast<std::size_t>(Source::kRDelay)] = rightDelayOut;

        std::array<float, static_cast<std::size_t>(Source::kCount)> raw = {
            leftPitchOut, rightPitchOut, leftDelayOut, rightDelayOut
        };

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (std::size_t i = 0; i < raw.size(); ++i)
        {
            auto value = raw[i] * levels_[i];
            auto panL = 0.5f * (1.0f - pans_[i]);
            auto panR = 0.5f * (1.0f + pans_[i]);
            wetLeft += value * panL;
            wetRight += value * panR;
        }

        rotateStereoWidth(wetLeft, wetRight, imageDegrees_);

        left = lerp(dryLeft, wetLeft, mix_);
        right = lerp(dryRight, wetRight, mix_);
    }

  private:
    static float clampPercent(float percent) { return std::clamp(percent, -100.0f, 100.0f) / 100.0f; }

    // Wraps ReverseBuffer with an enabled flag so Direction can toggle
    // between it and the plain pitch-delay path without allocating a
    // second set of buffers.
    struct ToggleableReverse
    {
        ReverseBuffer buffer;
        bool enabled = false;

        void setBuffer(std::span<float> b) { buffer.setBuffer(b); }
        void prepare(float sampleRate) { buffer.prepare(sampleRate); }
        void setLengthSeconds(float seconds) { buffer.setLengthSeconds(seconds); }
        void reset() { buffer.reset(); }
        float process(float input) { return buffer.process(input); }
    };

    float sampleRate_ = 48000.0f;

    DelayLine leftPitchDelay_, rightPitchDelay_;
    ToggleableReverse leftReverse_, rightReverse_;
    PitchShifter leftShifter_, rightShifter_;
    DelayLine leftDryDelay_, rightDryDelay_;

    float leftPitchDelaySamples_ = 0.0f, rightPitchDelaySamples_ = 0.0f;
    float leftDelaySamples_ = 0.0f, rightDelaySamples_ = 0.0f;

    float mix_ = 1.0f;
    float feedbackScale_ = 0.0f;
    float imageDegrees_ = 0.0f;

    std::array<float, static_cast<std::size_t>(Source::kCount)> levels_{};
    std::array<float, static_cast<std::size_t>(Source::kCount)> pans_{};

    float leftFb1Amount_ = 0.0f, leftFb2Amount_ = 0.0f;
    Source leftFb1Source_ = Source::kLPitch, leftFb2Source_ = Source::kLDelay;
    float rightFb1Amount_ = 0.0f, rightFb2Amount_ = 0.0f;
    Source rightFb1Source_ = Source::kRPitch, rightFb2Source_ = Source::kRDelay;

    std::array<float, static_cast<std::size_t>(Source::kCount)> previous_{};
};
}
