#pragma once

#include "dsp/AllpassFilter.h"
#include "dsp/LFO.h"
#include "dsp/Math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Phaser" (Algorithm 119), per that algorithm's
 * own manual page: "a mono-in, stereo-out phase shifter, similar in
 * theory to a guitarist's foot-pedal phaser. The dry signal is mixed
 * with the phase-shifted signal (created by a series of all-pass
 * filters) to produce a series of notches, whose frequencies can be
 * swept by altering the filter characteristics."
 *
 * Twelve `AllpassFilter` stages (see dsp/AllpassFilter.h - a genuinely
 * new Primitive, since this is a frequency-domain phase-shift filter,
 * not the fixed-delay Schroeder allpass the rest of this archive calls
 * `Allpass`) run in series, all twelve tuned to the same
 * continuously-swept corner frequency. Per the manual's own Block
 * Diagram, the output is asymmetric between channels:
 * `left = dry*(1-mix) + wet*mix`, `right = wet*mix` (no dry term at
 * all) - the notch-comb interaction only happens on the left, giving
 * the classic phaser stereo motion from a mono source.
 *
 * The sweep source is one of three (#5 Sweep Mode): a free-running LFO,
 * an envelope follower tracking the input's amplitude, or a simple ADSR
 * that can auto-cycle entirely from the envelope follower crossing
 * Attack/Release Thresholds (#12/#13) - "If a MIDI trigger is received
 * [during a segment], it will just continue/immediately reenter..."
 * describes MIDI as an *additional* way to retrigger the ADSR, not the
 * only one, so all three modes work fully without MIDI (no consumer in
 * this project implements MIDI input at all). `trigger()` stands in for
 * the manual's MIDI-only ADSR Trigger (#14), wired to a manual gesture
 * like every other trigger-driven algorithm in this archive.
 */
class Phaser
{
  public:
    static constexpr int kNumStages = 12;

    enum class SweepMode
    {
        kLfo,
        kEnvelope,
        kAdsr,
    };

    static constexpr std::size_t requiredWorkingBufferSize() { return 0; }

    void prepare(float sampleRate, std::span<float> /* workingBuffer */)
    {
        sampleRate_ = sampleRate;

        setMix(50.0f);
        setFeedback(0.0f);
        setSweepRate(50.0f);
        setEnvelopeDecayRate(50.0f);
        setAdsrRateScaler(100.0f);
        setSweepMode(SweepMode::kLfo);
        setSweepBottom(20.0f);
        setSweepTop(60.0f);
        setAdsrAttackRate(60.0f);
        setAdsrDecayRate(50.0f);
        setAdsrSustainLevel(60.0f);
        setAdsrReleaseRate(40.0f);
        setAdsrAttackThreshold(30.0f);
        setAdsrReleaseThreshold(10.0f);
        setEnvelopeChannel(false);
        setEnvelopeDecayShapeExponential(true);

        reset();
    }

    // #0: 0-100%. Left = dry*(1-mix) + wet*mix; Right = wet*mix (no dry).
    void setMix(float percent0to100) { mix_ = clamp01(percent0to100 / 100.0f); }
    // #1: -100..100%. Feeds the phase-shifted chain's own output back into
    // its input; at +/-100% no dry signal is admitted and the loop resonates.
    void setFeedback(float percent) { feedback_ = std::clamp(percent, -100.0f, 100.0f) / 100.0f; }
    // #2: 0-100%, LFO mode's sweep rate.
    void setSweepRate(float percent0to100)
    {
        auto hz = mapLinear(percent0to100 / 100.0f, 0.02f, 6.0f);
        lfo_.setFrequency(hz, sampleRate_);
    }
    // #3: 0-100%, the envelope follower's decay speed (instantaneous rise).
    void setEnvelopeDecayRate(float percent0to100)
    {
        auto seconds = mapLinear(1.0f - percent0to100 / 100.0f, 0.01f, 3.0f);
        envelopeDecayCoefficient_ = timeConstantToCoefficient(seconds, sampleRate_);
        envelopeLinearStep_ = 1.0f / std::max(seconds * sampleRate_, 1.0f);
    }
    // #4: 0-100%, scales the ADSR's Attack/Decay/Release rates.
    void setAdsrRateScaler(float percent0to100) { adsrRateScaler_ = clamp01(percent0to100 / 100.0f); }
    // #5: which source drives the allpass sweep frequency.
    void setSweepMode(SweepMode mode) { sweepMode_ = mode; }
    // #6/#7: 0-100%, the two extremes of the sweep (log-mapped to Hz); Bottom
    // may exceed Top to invert the sweep direction, per the manual.
    void setSweepBottom(float percent0to100) { sweepBottomHz_ = percentToHz(percent0to100); }
    void setSweepTop(float percent0to100) { sweepTopHz_ = percentToHz(percent0to100); }

    // #8-11 (expert): ADSR segment rates and sustain level.
    void setAdsrAttackRate(float percent0to100) { adsrAttackRatePerSecond_ = rateToPerSecond(percent0to100); }
    void setAdsrDecayRate(float percent0to100) { adsrDecayRatePerSecond_ = rateToPerSecond(percent0to100); }
    void setAdsrSustainLevel(float percent0to100) { adsrSustainLevel_ = clamp01(percent0to100 / 100.0f); }
    void setAdsrReleaseRate(float percent0to100) { adsrReleaseRatePerSecond_ = rateToPerSecond(percent0to100); }
    // #12/#13 (expert): envelope-follower thresholds that auto-cycle the ADSR.
    void setAdsrAttackThreshold(float percent0to100) { adsrAttackThreshold_ = clamp01(percent0to100 / 100.0f); }
    void setAdsrReleaseThreshold(float percent0to100) { adsrReleaseThreshold_ = clamp01(percent0to100 / 100.0f); }
    // #15 (expert): false = envelope follows the (left) signal being
    // phase-shifted; true = envelope follows the right input instead, as a
    // sidechain that doesn't itself get phase-shifted.
    void setEnvelopeChannel(bool useRightAsSidechain) { envelopeUsesRightChannel_ = useRightAsSidechain; }
    // #16 (expert): true = exponential decay, false = linear.
    void setEnvelopeDecayShapeExponential(bool exponential) { envelopeDecayExponential_ = exponential; }

    // Manual substitute for the MIDI-only ADSR Trigger (#14): restarts the
    // ADSR into its attack segment immediately, from any state.
    void trigger()
    {
        adsrState_ = AdsrState::kAttack;
    }

    void reset()
    {
        for (auto& stage : stages_)
        {
            stage.reset();
        }
        lfo_.setPhase(0.0f);
        envelopeLevel_ = 0.0f;
        adsrLevel_ = 0.0f;
        adsrState_ = AdsrState::kRelease;
        previousChainOutput_ = 0.0f;
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
        auto dry = left; // mono-in per the manual's own Block Diagram.
        auto sidechain = envelopeUsesRightChannel_ ? right : dry;

        updateEnvelopeFollower(sidechain);
        updateAdsr();

        auto sweepValue = sweepMode_ == SweepMode::kLfo ? 0.5f + 0.5f * lfo_.nextSine()
                           : sweepMode_ == SweepMode::kEnvelope ? envelopeLevel_
                                                                 : adsrLevel_;
        auto cutoffHz = mapLinear(sweepValue, sweepBottomHz_, sweepTopHz_);

        auto chainInput = dry + feedback_ * previousChainOutput_;
        auto wet = chainInput;
        for (auto& stage : stages_)
        {
            stage.setCutoff(cutoffHz, sampleRate_);
            wet = stage.process(wet);
        }
        previousChainOutput_ = wet;

        left = dry * (1.0f - mix_) + wet * mix_;
        right = wet * mix_;
    }

  private:
    enum class AdsrState
    {
        kAttack,
        kDecay,
        kSustain,
        kRelease,
    };

    static float percentToHz(float percent0to100)
    {
        constexpr float kMinHz = 20.0f;
        constexpr float kMaxHz = 15000.0f;
        auto t = clamp01(percent0to100 / 100.0f);
        return kMinHz * std::pow(kMaxHz / kMinHz, t);
    }

    float rateToPerSecond(float percent0to100) const
    {
        // Higher rate% = faster = a shorter full 0->1 sweep.
        auto fullSweepSeconds = mapLinear(1.0f - percent0to100 / 100.0f, 0.005f, 8.0f);
        return 1.0f / fullSweepSeconds;
    }

    void updateEnvelopeFollower(float input)
    {
        auto rectified = std::fabs(input);
        if (rectified > envelopeLevel_)
        {
            envelopeLevel_ = rectified;
        }
        else if (envelopeDecayExponential_)
        {
            envelopeLevel_ *= envelopeDecayCoefficient_;
        }
        else
        {
            envelopeLevel_ = std::max(0.0f, envelopeLevel_ - envelopeLinearStep_);
        }
        envelopeLevel_ = clamp01(envelopeLevel_);
    }

    void updateAdsr()
    {
        auto step = adsrRateScaler_ / sampleRate_;
        switch (adsrState_)
        {
            case AdsrState::kAttack:
                adsrLevel_ += adsrAttackRatePerSecond_ * step;
                if (adsrLevel_ >= 1.0f)
                {
                    adsrLevel_ = 1.0f;
                    adsrState_ = AdsrState::kDecay;
                }
                break;
            case AdsrState::kDecay:
                adsrLevel_ -= adsrDecayRatePerSecond_ * step;
                if (adsrLevel_ <= adsrSustainLevel_)
                {
                    adsrLevel_ = adsrSustainLevel_;
                    adsrState_ = AdsrState::kSustain;
                }
                break;
            case AdsrState::kSustain:
                if (envelopeLevel_ < adsrReleaseThreshold_)
                {
                    adsrState_ = AdsrState::kRelease;
                }
                break;
            case AdsrState::kRelease:
                adsrLevel_ -= adsrReleaseRatePerSecond_ * step;
                if (adsrLevel_ <= 0.0f)
                {
                    adsrLevel_ = 0.0f;
                }
                if (envelopeLevel_ > adsrAttackThreshold_)
                {
                    adsrState_ = AdsrState::kAttack;
                }
                break;
        }
        adsrLevel_ = clamp01(adsrLevel_);
    }

    float sampleRate_ = 48000.0f;

    std::array<AllpassFilter, kNumStages> stages_;
    LFO lfo_;

    float mix_ = 0.5f;
    float feedback_ = 0.0f;
    float previousChainOutput_ = 0.0f;

    SweepMode sweepMode_ = SweepMode::kLfo;
    float sweepBottomHz_ = 100.0f;
    float sweepTopHz_ = 2000.0f;

    float envelopeLevel_ = 0.0f;
    float envelopeDecayCoefficient_ = 0.999f;
    float envelopeLinearStep_ = 0.0001f;
    bool envelopeDecayExponential_ = true;
    bool envelopeUsesRightChannel_ = false;

    AdsrState adsrState_ = AdsrState::kRelease;
    float adsrLevel_ = 0.0f;
    float adsrRateScaler_ = 1.0f;
    float adsrAttackRatePerSecond_ = 1.0f;
    float adsrDecayRatePerSecond_ = 1.0f;
    float adsrSustainLevel_ = 0.6f;
    float adsrReleaseRatePerSecond_ = 1.0f;
    float adsrAttackThreshold_ = 0.3f;
    float adsrReleaseThreshold_ = 0.1f;
};
}
