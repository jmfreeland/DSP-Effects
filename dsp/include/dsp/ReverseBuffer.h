#pragma once

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp
{
/**
 * A "tape reverse" splice generator: records a settable-length segment
 * while simultaneously playing back the *previous* segment in reverse,
 * swapping roles each time the recording segment fills - the mechanism
 * behind the Eventide H3000's Reverse Shift (Algorithm 104): "Think of a
 * tape recorder recording a small length of tape (time)... and then
 * playing it back in reverse while it records the next." See
 * docs/eventide-reverse-shift.md.
 *
 * Two fixed-capacity halves ping-pong roles (recording/playback) rather
 * than one circular buffer, since true reversal needs a *stable* segment
 * to read backward through while a *different* region keeps recording -
 * unlike PitchShifter's forward-sweeping taps, which can share one
 * circular buffer because they only ever read behind the write head.
 */
class ReverseBuffer
{
  public:
    static constexpr std::size_t requiredCapacity(float maxLengthSeconds,
                                                    float maxSampleRate = 96000.0f)
    {
        return 2 * static_cast<std::size_t>(maxLengthSeconds * maxSampleRate);
    }

    // buffer must be at least requiredCapacity(maxLengthSeconds) long;
    // split evenly into the two ping-pong halves.
    void setBuffer(std::span<float> buffer)
    {
        auto half = buffer.size() / 2;
        halfA_ = buffer.subspan(0, half);
        halfB_ = buffer.subspan(half, half);
    }

    void prepare(float sampleRate)
    {
        sampleRate_ = sampleRate;
        reset();
    }

    // 0 (a click's worth) .. capacity: the splice length - matches the
    // manual's own Length parameter. Takes effect at the next swap
    // boundary, not mid-segment, to avoid corrupting the segment
    // currently being read backward.
    void setLengthSeconds(float seconds)
    {
        auto capacity = static_cast<float>(halfA_.size());
        pendingLengthSamples_ =
          static_cast<std::size_t>(std::clamp(seconds * sampleRate_, 2.0f, capacity));
    }

    void reset()
    {
        for (auto& s : halfA_) s = 0.0f;
        for (auto& s : halfB_) s = 0.0f;
        aIsRecording_ = true;
        index_ = 0;
        lengthSamples_ = pendingLengthSamples_;
    }

    float process(float input)
    {
        auto& recordHalf = aIsRecording_ ? halfA_ : halfB_;
        auto& playHalf = aIsRecording_ ? halfB_ : halfA_;

        recordHalf[index_] = input;
        // Playing back the *previous* segment in reverse: index_ counts
        // up while recording, so mirror it to count down for playback.
        auto playIndex = lengthSamples_ - 1 - index_;
        auto output = playIndex < playHalf.size() ? playHalf[playIndex] : 0.0f;

        if (++index_ >= lengthSamples_)
        {
            index_ = 0;
            aIsRecording_ = !aIsRecording_;
            lengthSamples_ = pendingLengthSamples_;
        }

        return output;
    }

  private:
    std::span<float> halfA_;
    std::span<float> halfB_;
    float sampleRate_ = 48000.0f;
    bool aIsRecording_ = true;
    std::size_t index_ = 0;
    std::size_t lengthSamples_ = 2205;        // 50ms @ 44.1kHz-ish default
    std::size_t pendingLengthSamples_ = 2205;
};
}
