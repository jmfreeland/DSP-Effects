#pragma once

#include "dsp/Comb.h"
#include "dsp/Envelope.h"
#include "dsp/OnePole.h"
#include "dsp/algorithms/ReverbCore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * A Lexicon PCM81-inspired "Plate": high initial diffusion and a bright,
 * even tail, good on percussion. Built on the shared ReverbCore signal
 * path (see ReverbCore.h), adding the two things the manual calls out as
 * Plate-specific:
 *
 *  - EkoDly/EkoFbk: a recirculating stereo pre-echo (dsp::Comb per
 *    channel) mixed additively into the input ahead of PreDelay/Diffusion
 *    - the manual's "pre-echo w/ feedback," shared with Chamber/Infinite.
 *  - Attack: sharpness of the initial response, first ~50ms only. An
 *    original reconstruction (the manual gives no further detail): a
 *    transient detector (input level vs. a slow follower) retriggers a
 *    50ms LinearRamp on each new onset; while active, it temporarily
 *    lowers the effective Diffusion coefficient toward a soft floor, then
 *    releases back to the set Diffusion amount. Attack=1 disables the
 *    dip entirely (immediate full density = "sharp"); Attack=0 makes the
 *    dip deepest (density visibly builds over the 50ms window = "soft").
 */
class Plate : public ReverbCore
{
  public:
    static constexpr int kEkoCapacitySamples = kEarlyReflectionCapacitySamples; // 1.2s @ 48kHz, matches RefDly

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return ReverbCore::requiredWorkingBufferSize() + 2 * static_cast<std::size_t>(kEkoCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        auto coreSize = ReverbCore::requiredWorkingBufferSize();
        ReverbCore::prepare(sampleRate, workingBuffer.first(coreSize));

        auto remaining = workingBuffer.subspan(coreSize);
        ekoLeft_.setBuffer(remaining.subspan(0, static_cast<std::size_t>(kEkoCapacitySamples)));
        ekoRight_.setBuffer(
          remaining.subspan(static_cast<std::size_t>(kEkoCapacitySamples), static_cast<std::size_t>(kEkoCapacitySamples)));

        attackWindowSamples_ = 0.05f * sampleRate;
        transientFollower_.setCoefficient(onePoleLowpassCoefficient(15.0f, sampleRate));

        // Plate's own character: brighter and denser than Concert Hall,
        // with a moderate pre-echo and a fairly sharp attack by default.
        setDiffusion(0.85f);
        setDamping(0.25f);
        setEkoDelaySeconds(0.045f, 0.055f);
        setEkoFeedback(0.25f, 0.25f);
        setAttack(0.6f);
        reset();
    }

    // 0..1 level of each channel's recirculating pre-echo tap.
    void setEkoFeedback(float left, float right)
    {
        ekoLeft_.setFeedback(std::clamp(left, 0.0f, 0.95f));
        ekoRight_.setFeedback(std::clamp(right, 0.0f, 0.95f));
    }

    // Delay of each channel's pre-echo tap, 0..1.2s.
    void setEkoDelaySeconds(float left, float right)
    {
        ekoLeft_.setDelaySamples(
          std::clamp(left * sampleRate(), 0.0f, static_cast<float>(kEkoCapacitySamples - 2)));
        ekoRight_.setDelaySamples(
          std::clamp(right * sampleRate(), 0.0f, static_cast<float>(kEkoCapacitySamples - 2)));
    }

    // 0 (soft: density visibly builds over the first 50ms) .. 1 (sharp:
    // full density immediately).
    void setAttack(float amount) { attackAmount_ = std::clamp(amount, 0.0f, 1.0f); }

    void reset()
    {
        ReverbCore::reset();
        ekoLeft_.reset();
        ekoRight_.reset();
        transientFollower_.reset();
        attackRamp_.reset();
        attackEnvelope_ = 0.0f;
        wasTransient_ = false;
    }

  protected:
    void applyPreEcho(float& left, float& right) override
    {
        auto level = 0.5f * (std::fabs(left) + std::fabs(right));
        auto followed = transientFollower_.process(level);
        // Rising-edge trigger only: a sustained loud passage keeps
        // level > followed for as long as it holds, but should retrigger
        // the attack window once at its onset, not every sample.
        auto isTransient = level > followed * 1.6f + 0.001f;
        if (isTransient && !wasTransient_)
        {
            attackRamp_.trigger(1.0f, 0.0f, attackWindowSamples_);
        }
        wasTransient_ = isTransient;
        attackEnvelope_ = attackRamp_.next();

        left += ekoLeft_.process(left);
        right += ekoRight_.process(right);
    }

    float effectiveDiffusion(float baseAmount) override
    {
        constexpr float kFloorRatio = 0.15f;
        auto dipDepth = (1.0f - attackAmount_) * baseAmount * (1.0f - kFloorRatio);
        return baseAmount - dipDepth * attackEnvelope_;
    }

  private:
    Comb ekoLeft_;
    Comb ekoRight_;

    OnePoleLowpass transientFollower_;
    LinearRamp attackRamp_;
    bool wasTransient_ = false;
    float attackEnvelope_ = 0.0f;
    float attackWindowSamples_ = 2400.0f;
    float attackAmount_ = 0.6f;
};
}
