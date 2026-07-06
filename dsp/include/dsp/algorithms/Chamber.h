#pragma once

#include "dsp/Comb.h"
#include "dsp/Envelope.h"
#include "dsp/Math.h"
#include "dsp/OnePole.h"
#include "dsp/algorithms/ReverbCore.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * A Lexicon PCM81-inspired "Chamber": even and "dimensionless," with
 * little color change over the decay - good on vocals. Built on the
 * shared ReverbCore signal path (see ReverbCore.h), adding the things
 * the manual calls out as Chamber-specific:
 *
 *  - EkoDly/EkoFbk: the same recirculating stereo pre-echo as Plate (see
 *    Plate.h) - shared mechanism, different default tuning (more subtle
 *    here, matching Chamber's even character rather than Plate's
 *    percussive one).
 *  - Shape + Spread: "envelope contour" + "sustain." An original
 *    reconstruction (the manual gives no further detail): a transient
 *    detector (same rising-edge technique as Plate's Attack) retriggers
 *    a two-stage gain envelope applied to the final wet output via the
 *    outputEnvelope() hook. Shape sets how far above unity the envelope
 *    swells on a new onset and how fast (0 = no swell/immediate; 1 = a
 *    pronounced, slower swell). Spread sets how long that swell takes to
 *    relax back down to unity (0 = quick; 1 = up to 3s) - during the
 *    relax, the tank's own ongoing decay is heard at an elevated level,
 *    reading as extended sustain.
 */
class Chamber : public ReverbCore
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

        transientFollower_.setCoefficient(onePoleLowpassCoefficient(15.0f, sampleRate));

        // Chamber's own character: even and dimensionless, so more
        // moderate diffusion/damping than Plate, and a subtler pre-echo.
        setDiffusion(0.5f);
        setDamping(0.4f);
        setEkoDelaySeconds(0.06f, 0.07f);
        setEkoFeedback(0.2f, 0.2f);
        setShape(0.3f);
        setSpread(0.4f);
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

    // 0 (no swell: onset reads as immediate/flat) .. 1 (pronounced,
    // slower swell on each new onset).
    void setShape(float amount) { shapeAmount_ = std::clamp(amount, 0.0f, 1.0f); }

    // 0 (swell relaxes quickly) .. 1 (swell lingers up to ~3s, read as
    // extended sustain).
    void setSpread(float amount) { spreadAmount_ = std::clamp(amount, 0.0f, 1.0f); }

    void reset()
    {
        ReverbCore::reset();
        ekoLeft_.reset();
        ekoRight_.reset();
        transientFollower_.reset();
        envelopeRamp_.reset();
        envelopeStage_ = Stage::kIdle;
        envelopeValue_ = 1.0f;
        wasTransient_ = false;
    }

  protected:
    void applyPreEcho(float& left, float& right) override
    {
        auto level = 0.5f * (std::fabs(left) + std::fabs(right));
        auto followed = transientFollower_.process(level);
        // Rising-edge trigger only: a sustained loud passage keeps
        // level > followed for as long as it holds, but should retrigger
        // the swell once at its onset, not every sample.
        auto isTransient = level > followed * 1.6f + 0.001f;
        if (isTransient && !wasTransient_)
        {
            auto peak = 1.0f + shapeAmount_ * 1.5f;
            auto attackSamples = mapLinear(shapeAmount_, 0.005f, 0.3f) * sampleRate();
            envelopeRamp_.trigger(envelopeValue_, peak, attackSamples);
            envelopeStage_ = Stage::kAttack;
        }
        wasTransient_ = isTransient;

        if (envelopeStage_ == Stage::kAttack)
        {
            envelopeValue_ = envelopeRamp_.next();
            if (!envelopeRamp_.isActive())
            {
                auto releaseSamples = mapLinear(spreadAmount_, 0.05f, 3.0f) * sampleRate();
                envelopeRamp_.trigger(envelopeValue_, 1.0f, releaseSamples);
                envelopeStage_ = Stage::kRelease;
            }
        }
        else if (envelopeStage_ == Stage::kRelease)
        {
            envelopeValue_ = envelopeRamp_.next();
            if (!envelopeRamp_.isActive())
            {
                envelopeStage_ = Stage::kIdle;
                envelopeValue_ = 1.0f;
            }
        }

        left += ekoLeft_.process(left);
        right += ekoRight_.process(right);
    }

    float outputEnvelope() override { return envelopeValue_; }

  private:
    enum class Stage
    {
        kIdle,
        kAttack,
        kRelease
    };

    Comb ekoLeft_;
    Comb ekoRight_;

    OnePoleLowpass transientFollower_;
    LinearRamp envelopeRamp_;
    Stage envelopeStage_ = Stage::kIdle;
    float envelopeValue_ = 1.0f;
    bool wasTransient_ = false;
    float shapeAmount_ = 0.3f;
    float spreadAmount_ = 0.4f;
};
}
