#pragma once

#include "dsp/DelayLine.h"
#include "dsp/GlideParameter.h"
#include "dsp/Math.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Long Digiplex" - Algorithm 109 (see
 * docs/eventide-h3000-notes.md and docs/eventide-long-digiplex.md). Per
 * the manual: "Algorithm 109 is one long delay line capable of
 * recirculating its output back to its input. The output is sent to
 * both right and left channels." The simplest H3000 algorithm in this
 * archive - a single delay line with feedback, reusing `GlideParameter`
 * (already built for the Lexicon PCM81's own Voice/Post-Delay glide)
 * for the manual's own Glide Speed/Enable behavior on Delay changes.
 */
class LongDigiplex
{
  public:
    // Matches Algorithm 109's own documented Delay parameter range.
    static constexpr float kMaxDelaySeconds = 1.4f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize() { return kDelayCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        delay_.setBuffer(workingBuffer);
        delayGlide_.setSampleRate(sampleRate_);

        setDelaySeconds(0.3f);
        setFeedback(0.0f);
        setGlide(50.0f, true);
        setRepeat(false);
        setMix(0.5f);
        reset();
    }

    // 0-1.4s.
    void setDelaySeconds(float seconds)
    {
        auto samples = std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples - 2));
        delayGlide_.setTarget(samples);
    }

    // -1..0.99: negative reverses feedback phase, matching the manual's
    // own Feedback range ("-100 to 99 per cent").
    void setFeedback(float feedbackMinus1to1) { feedback_ = std::clamp(feedbackMinus1to1, -1.0f, 0.99f); }

    // response 0..100, enabled matches the manual's Glide Speed/Enable
    // (glide "is normally on").
    void setGlide(float response0to100, bool enabled)
    {
        delayGlide_.setResponse(response0to100);
        delayGlide_.setRangeSeconds(enabled ? kMaxDelaySeconds : 0.0f);
    }

    // Captures up to 1.4s of audio and replays it continuously, muting
    // new input - matches the manual's Repeat control.
    void setRepeat(bool repeat) { repeat_ = repeat; }

    void setMix(float wet) { mix_ = clamp01(wet); }

    void reset()
    {
        delay_.reset();
        delayGlide_.reset();
        feedbackState_ = 0.0f;
    }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    // Left In only, per the manual's own "the output is sent to both
    // right and left channels" - Right In isn't part of the signal path.
    void processSample(float& left, float& right)
    {
        auto dryLeft = left;
        auto dryRight = right;

        auto input = repeat_ ? 0.0f : dryLeft;
        delay_.write(input + feedback_ * feedbackState_);
        auto delayed = delay_.readLinear(delayGlide_.next());
        feedbackState_ = delayed;

        left = lerp(dryLeft, delayed, mix_);
        right = lerp(dryRight, delayed, mix_);
    }

  private:
    DelayLine delay_;
    GlideParameter delayGlide_;
    float sampleRate_ = 48000.0f;
    float feedback_ = 0.0f;
    float feedbackState_ = 0.0f;
    float mix_ = 0.5f;
    bool repeat_ = false;
};
}
