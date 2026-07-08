#pragma once

#include "dsp/algorithms/Phaser.h"

#include <cstddef>
#include <span>

namespace dsp::graphs
{
/**
 * The Eventide H3000-inspired "Phaser" algorithm: the Phaser Block (see
 * dsp/algorithms/Phaser.h) plus independent Left/Right input trim. Right
 * input trim matters even though only Left is phase-shifted, since the
 * Block can optionally use the Right input as an unshifted envelope-
 * follower sidechain (Envelope Channel, #15).
 */
class PhaserAlgorithm
{
  public:
    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return dsp::algorithms::Phaser::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        engine_.prepare(sampleRate, workingBuffer);
        setInLevel(1.0f, 1.0f);
        reset();
    }

    // -- Phaser Block pass-throughs --
    void setMix(float percent0to100) { engine_.setMix(percent0to100); }
    void setFeedback(float percent) { engine_.setFeedback(percent); }
    void setSweepRate(float percent0to100) { engine_.setSweepRate(percent0to100); }
    void setEnvelopeDecayRate(float percent0to100) { engine_.setEnvelopeDecayRate(percent0to100); }
    void setAdsrRateScaler(float percent0to100) { engine_.setAdsrRateScaler(percent0to100); }
    void setSweepMode(dsp::algorithms::Phaser::SweepMode mode) { engine_.setSweepMode(mode); }
    void setSweepBottom(float percent0to100) { engine_.setSweepBottom(percent0to100); }
    void setSweepTop(float percent0to100) { engine_.setSweepTop(percent0to100); }
    void setAdsrAttackRate(float percent0to100) { engine_.setAdsrAttackRate(percent0to100); }
    void setAdsrDecayRate(float percent0to100) { engine_.setAdsrDecayRate(percent0to100); }
    void setAdsrSustainLevel(float percent0to100) { engine_.setAdsrSustainLevel(percent0to100); }
    void setAdsrReleaseRate(float percent0to100) { engine_.setAdsrReleaseRate(percent0to100); }
    void setAdsrAttackThreshold(float percent0to100) { engine_.setAdsrAttackThreshold(percent0to100); }
    void setAdsrReleaseThreshold(float percent0to100) { engine_.setAdsrReleaseThreshold(percent0to100); }
    void setEnvelopeChannel(bool useRightAsSidechain) { engine_.setEnvelopeChannel(useRightAsSidechain); }
    void setEnvelopeDecayShapeExponential(bool exponential) { engine_.setEnvelopeDecayShapeExponential(exponential); }
    void trigger() { engine_.trigger(); }

    // -- Input conditioning --
    void setInLevel(float left, float right)
    {
        inLevelLeft_ = left;
        inLevelRight_ = right;
    }

    void reset() { engine_.reset(); }

    void process(std::span<float> left, std::span<float> right)
    {
        for (std::size_t n = 0; n < left.size(); ++n)
        {
            processSample(left[n], right[n]);
        }
    }

    void processSample(float& left, float& right)
    {
        left *= inLevelLeft_;
        right *= inLevelRight_;
        engine_.processSample(left, right);
    }

  private:
    dsp::algorithms::Phaser engine_;
    float inLevelLeft_ = 1.0f;
    float inLevelRight_ = 1.0f;
};
}
