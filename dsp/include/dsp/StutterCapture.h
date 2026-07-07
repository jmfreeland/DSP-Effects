#pragma once

#include "dsp/DelayLine.h"

#include <cstddef>
#include <span>

namespace dsp
{
/**
 * The "Stutter Control" block from the Eventide H3000's Stutter
 * algorithm (#112): continuously records its input into a circular
 * buffer, and on trigger() freezes recording, then replays the most
 * recently recorded window (lengthSamples long) forward from its start
 * repeatedly (count times) before resuming live passthrough - the
 * "st..st..stutter" effect, without needing a sampler.
 *
 * Because writes are paused for the whole stutter (rather than
 * continuing to record over the captured window), repeated reads at the
 * same set of delay offsets reproduce the identical captured audio each
 * pass - the loop is exact, not drifting.
 */
class StutterCapture
{
  public:
    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }

    void reset()
    {
        delay_.reset();
        active_ = false;
    }

    bool isActive() const { return active_; }

    // Starts (or restarts) a stutter: replays the most recent
    // lengthSamples of already-recorded audio, count times. A
    // lengthSamples or count of 0 is a no-op.
    void trigger(float lengthSamples, int count)
    {
        if (lengthSamples < 1.0f || count <= 0)
        {
            return;
        }
        lengthSamples_ = lengthSamples;
        passesRemaining_ = count;
        readOffset_ = 0.0f;
        active_ = true;
    }

    float process(float input)
    {
        if (!active_)
        {
            delay_.write(input);
            return input;
        }

        auto delaySamples = (lengthSamples_ - 1.0f) - readOffset_;
        auto out = delay_.read(static_cast<std::size_t>(delaySamples));

        readOffset_ += 1.0f;
        if (readOffset_ >= lengthSamples_)
        {
            readOffset_ = 0.0f;
            --passesRemaining_;
            if (passesRemaining_ <= 0)
            {
                active_ = false;
                delay_.write(input);
            }
        }
        return out;
    }

  private:
    DelayLine delay_;
    float lengthSamples_ = 0.0f;
    float readOffset_ = 0.0f;
    int passesRemaining_ = 0;
    bool active_ = false;
};
}
