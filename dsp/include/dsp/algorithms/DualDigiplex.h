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
 * An Eventide H3000-inspired "Dual Digiplex" - Algorithm 110 (see
 * docs/eventide-h3000-notes.md and docs/eventide-dual-digiplex.md). Per
 * the manual: "Dual Digiplex is similar to Long Digiplex (Algorithm 109)
 * it provides two separate delay lines each with its own controls.
 * Delay time on each channel is up to .7 seconds." Two independent
 * copies of Long Digiplex's own "Delay + Feedback + Glide" shape (see
 * `dsp/algorithms/LongDigiplex.h`), one per channel, each reading its
 * own input rather than a shared Left-only source.
 */
class DualDigiplex
{
  public:
    // Matches Algorithm 110's own documented Delay parameter range
    // (half of Long Digiplex's 1.4s).
    static constexpr float kMaxDelaySeconds = 0.7f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize() { return 2 * kDelayCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        leftDelay_.setBuffer(workingBuffer.subspan(0, kDelayCapacitySamples));
        rightDelay_.setBuffer(workingBuffer.subspan(kDelayCapacitySamples, kDelayCapacitySamples));
        leftGlide_.setSampleRate(sampleRate_);
        rightGlide_.setSampleRate(sampleRate_);

        setLeftDelaySeconds(0.2f);
        setRightDelaySeconds(0.3f);
        setLeftFeedback(0.0f);
        setRightFeedback(0.0f);
        setGlide(50.0f, true);
        setRepeat(false);
        setLeftMix(0.5f);
        setRightMix(0.5f);
        setStereoInput(true);
        reset();
    }

    // 0-0.7s, independent per channel.
    void setLeftDelaySeconds(float seconds)
    {
        auto samples = std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples - 2));
        leftGlide_.setTarget(samples);
    }
    void setRightDelaySeconds(float seconds)
    {
        auto samples = std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples - 2));
        rightGlide_.setTarget(samples);
    }

    // -1..0.99, independent per channel.
    void setLeftFeedback(float feedbackMinus1to1) { leftFeedback_ = std::clamp(feedbackMinus1to1, -1.0f, 0.99f); }
    void setRightFeedback(float feedbackMinus1to1)
    {
        rightFeedback_ = std::clamp(feedbackMinus1to1, -1.0f, 0.99f);
    }

    // Shared between both channels, matching the manual's own single
    // Glide Speed/Enable pair for this algorithm.
    void setGlide(float response0to100, bool enabled)
    {
        leftGlide_.setResponse(response0to100);
        rightGlide_.setResponse(response0to100);
        leftGlide_.setRangeSeconds(enabled ? kMaxDelaySeconds : 0.0f);
        rightGlide_.setRangeSeconds(enabled ? kMaxDelaySeconds : 0.0f);
    }

    void setRepeat(bool repeat) { repeat_ = repeat; }

    void setLeftMix(float wet) { leftMix_ = clamp01(wet); }
    void setRightMix(float wet) { rightMix_ = clamp01(wet); }

    // true: two fully independent channels (each uses its own input).
    // false: both delay lines read the left input alone, matching the
    // manual: "If set to mono the signal at the left input channel can
    // have two separate delay times, each with its own feedback and mix."
    void setStereoInput(bool stereo) { stereoInput_ = stereo; }

    void reset()
    {
        leftDelay_.reset();
        rightDelay_.reset();
        leftGlide_.reset();
        rightGlide_.reset();
        leftFeedbackState_ = 0.0f;
        rightFeedbackState_ = 0.0f;
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

        auto leftIn = repeat_ ? 0.0f : dryLeft;
        auto rightIn = repeat_ ? 0.0f : (stereoInput_ ? dryRight : dryLeft);

        leftDelay_.write(leftIn + leftFeedback_ * leftFeedbackState_);
        auto leftDelayed = leftDelay_.readLinear(leftGlide_.next());
        leftFeedbackState_ = leftDelayed;

        rightDelay_.write(rightIn + rightFeedback_ * rightFeedbackState_);
        auto rightDelayed = rightDelay_.readLinear(rightGlide_.next());
        rightFeedbackState_ = rightDelayed;

        left = lerp(dryLeft, leftDelayed, leftMix_);
        right = lerp(dryRight, rightDelayed, rightMix_);
    }

  private:
    DelayLine leftDelay_;
    DelayLine rightDelay_;
    GlideParameter leftGlide_;
    GlideParameter rightGlide_;
    float sampleRate_ = 48000.0f;
    float leftFeedback_ = 0.0f;
    float rightFeedback_ = 0.0f;
    float leftFeedbackState_ = 0.0f;
    float rightFeedbackState_ = 0.0f;
    float leftMix_ = 0.5f;
    float rightMix_ = 0.5f;
    bool repeat_ = false;
    bool stereoInput_ = true;
};
}
