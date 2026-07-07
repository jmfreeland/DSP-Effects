#pragma once

#include "dsp/DelayLine.h"
#include "dsp/Math.h"
#include "dsp/PitchShifter.h"
#include "dsp/StutterCapture.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Stutter" (Algorithm 112): a real-time
 * "st..st..stutter" performance effect, per that algorithm's own manual
 * page: "used to create that popular st..st..stutter sound - in
 * real-time, without the need for a sampler or cumbersome digital delay
 * acrobatics." Per channel: Input -> (+ feedback) -> PitchShifter (base
 * Coarse/Fine plus whichever sweep generator(s) are targeting this
 * channel) -> DelayLine (own Delay/Feedback) -> StutterCapture (the
 * manual's own "Stutter Control" block - passthrough until triggered,
 * then loops its most recently recorded window).
 *
 * The manual's own trigger system is a full second patch-matrix (4
 * trigger keys, each patchable to 2 of ~15 stutter/sweep/random-pitch
 * variants with independent l/r/l&r targeting) layered on top of this
 * same signal path - since this archive already built one general patch
 * matrix for Patch Factory (#111), Stutter instead exposes a smaller,
 * fixed set of trigger *methods* directly (two stutter presets, two
 * sweep generators each with up/down/random-pitch actions, each firing
 * on both channels at once) - see docs/eventide-stutter.md for the
 * full reasoning and the resulting known simplifications.
 */
class Stutter
{
  public:
    static constexpr float kMaxDelaySeconds = 0.5f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kGrainCapacitySamples = kDelayCapacitySamples;

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        // Per channel: pitch-shifter grain buffer + delay line + stutter
        // capture buffer.
        return 2 * (kGrainCapacitySamples + kDelayCapacitySamples + kDelayCapacitySamples);
    }

    enum class SweepTarget
    {
        kNone,
        kLeft,
        kRight,
        kBoth
    };

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;

        std::size_t offset = 0;
        leftShifter_.setBuffer(workingBuffer.subspan(offset, kGrainCapacitySamples));
        offset += kGrainCapacitySamples;
        leftDelay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        leftStutter_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        rightShifter_.setBuffer(workingBuffer.subspan(offset, kGrainCapacitySamples));
        offset += kGrainCapacitySamples;
        rightDelay_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));
        offset += kDelayCapacitySamples;
        rightStutter_.setBuffer(workingBuffer.subspan(offset, kDelayCapacitySamples));

        leftShifter_.prepare(sampleRate_);
        rightShifter_.prepare(sampleRate_);

        setLength1(0.1f);
        setCount1(4);
        setLength2(0.05f);
        setCount2(8);
        setLeftCoarseFineCents(0.0f);
        setRightCoarseFineCents(0.0f);
        setLeftDelaySeconds(0.0f);
        setRightDelaySeconds(0.0f);
        setLeftFeedback(0.0f);
        setRightFeedback(0.0f);
        setUp1(50.0f, 100.0f);
        setDn1(50.0f, -100.0f);
        setRand1Max(1200.0f);
        setUp2(50.0f, 100.0f);
        setDn2(50.0f, -100.0f);
        setRand2Max(1200.0f);
        setSweepTarget1(SweepTarget::kLeft);
        setSweepTarget2(SweepTarget::kRight);
        setLeftMix(1.0f);
        setRightMix(1.0f);
        setAuto(false);
        setSpeed(50.0f);
        setProgram(Program::kTotalRandom);

        reset();
    }

    // -- Stutter capture presets (#13-16: Length1/2, Count1/2) --
    void setLength1(float seconds) { length1Samples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_; }
    void setLength2(float seconds) { length2Samples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_; }
    void setCount1(int count) { count1_ = std::clamp(count, 0, 16); }
    void setCount2(int count) { count2_ = std::clamp(count, 0, 16); }

    // -- Per-channel pitch shift + delay chain (#25-30) --
    void setLeftCoarseFineCents(float cents) { leftBaseCents_ = std::clamp(cents, -4800.0f, 1200.0f); }
    void setRightCoarseFineCents(float cents) { rightBaseCents_ = std::clamp(cents, -4800.0f, 1200.0f); }
    void setLeftDelaySeconds(float seconds)
    {
        leftDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setRightDelaySeconds(float seconds)
    {
        rightDelaySamples_ = std::clamp(seconds, 0.0f, kMaxDelaySeconds) * sampleRate_;
    }
    void setLeftFeedback(float percent) { leftFeedback_ = std::clamp(percent, 0.0f, 100.0f) / 100.0f; }
    void setRightFeedback(float percent) { rightFeedback_ = std::clamp(percent, 0.0f, 100.0f) / 100.0f; }

    // -- Sweep generators (#31-40) --
    // rate0to100: how fast the sweep moves toward its target, in cents/second-ish.
    void setUp1(float rate0to100, float maxCents) { up1Rate_ = rate0to100; up1Max_ = maxCents; }
    void setDn1(float rate0to100, float minCents) { dn1Rate_ = rate0to100; dn1Min_ = minCents; }
    void setUp2(float rate0to100, float maxCents) { up2Rate_ = rate0to100; up2Max_ = maxCents; }
    void setDn2(float rate0to100, float minCents) { dn2Rate_ = rate0to100; dn2Min_ = minCents; }
    void setRand1Max(float cents) { rand1Max_ = std::clamp(cents, -1200.0f, 1200.0f); }
    void setRand2Max(float cents) { rand2Max_ = std::clamp(cents, -1200.0f, 1200.0f); }
    void setSweepTarget1(SweepTarget target) { sweepTarget1_ = target; }
    void setSweepTarget2(SweepTarget target) { sweepTarget2_ = target; }

    // -- Mix (#7-8) --
    void setLeftMix(float wet) { leftMix_ = clamp01(wet); }
    void setRightMix(float wet) { rightMix_ = clamp01(wet); }

    // -- Auto trigger sequencer (#4-6) --
    enum class Program
    {
        kTotalRandom,
        kRandomSweep,
        kRandomPitch,
        kJustStutter
    };
    void setAuto(bool enabled) { autoEnabled_ = enabled; }
    void setSpeed(float speed0to100) { speed_ = std::clamp(speed0to100, 0.0f, 100.0f); }
    void setProgram(Program program) { program_ = program; }

    // -- Manual triggers (a representative subset of the manual's
    // Trigger List, each firing on both channels - see class doc). --
    void triggerStutter1()
    {
        leftStutter_.trigger(length1Samples_, count1_);
        rightStutter_.trigger(length1Samples_, count1_);
    }
    void triggerStutter2()
    {
        leftStutter_.trigger(length2Samples_, count2_);
        rightStutter_.trigger(length2Samples_, count2_);
    }
    void triggerSweepUp1() { sweep1_.startUp(up1Max_); }
    void triggerSweepDown1() { sweep1_.startDown(dn1Min_); }
    void triggerRandomPitch1() { sweep1_.jumpRandom(nextRandom(rand1Max_)); }
    void triggerSweepUp2() { sweep2_.startUp(up2Max_); }
    void triggerSweepDown2() { sweep2_.startDown(dn2Min_); }
    void triggerRandomPitch2() { sweep2_.jumpRandom(nextRandom(rand2Max_)); }

    void reset()
    {
        leftShifter_.reset();
        rightShifter_.reset();
        leftDelay_.reset();
        rightDelay_.reset();
        leftStutter_.reset();
        rightStutter_.reset();
        leftFeedbackState_ = 0.0f;
        rightFeedbackState_ = 0.0f;
        sweep1_ = Sweep{};
        sweep2_ = Sweep{};
        autoCountdownSamples_ = 0.0f;
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

        updateAutoSequencer();

        auto up1Speed = up1Rate_ / 100.0f * kSweepRateScale;
        auto dn1Speed = dn1Rate_ / 100.0f * kSweepRateScale;
        auto up2Speed = up2Rate_ / 100.0f * kSweepRateScale;
        auto dn2Speed = dn2Rate_ / 100.0f * kSweepRateScale;
        sweep1_.advance(sampleRate_, up1Speed, dn1Speed);
        sweep2_.advance(sampleRate_, up2Speed, dn2Speed);

        auto leftSweepCents = (sweepTarget1_ == SweepTarget::kLeft || sweepTarget1_ == SweepTarget::kBoth
                                  ? sweep1_.value
                                  : 0.0f) +
                              (sweepTarget2_ == SweepTarget::kLeft || sweepTarget2_ == SweepTarget::kBoth
                                 ? sweep2_.value
                                 : 0.0f);
        auto rightSweepCents = (sweepTarget1_ == SweepTarget::kRight || sweepTarget1_ == SweepTarget::kBoth
                                   ? sweep1_.value
                                   : 0.0f) +
                               (sweepTarget2_ == SweepTarget::kRight || sweepTarget2_ == SweepTarget::kBoth
                                  ? sweep2_.value
                                  : 0.0f);

        leftShifter_.setSemitones((leftBaseCents_ + leftSweepCents) / 100.0f);
        rightShifter_.setSemitones((rightBaseCents_ + rightSweepCents) / 100.0f);

        auto leftIn = dryLeft + leftFeedback_ * leftFeedbackState_;
        auto leftShifted = leftShifter_.process(leftIn);
        leftDelay_.write(leftShifted);
        auto leftDelayed = leftDelay_.readLinear(leftDelaySamples_);
        leftFeedbackState_ = leftDelayed;
        auto leftStuttered = leftStutter_.process(leftDelayed);

        auto rightIn = dryRight + rightFeedback_ * rightFeedbackState_;
        auto rightShifted = rightShifter_.process(rightIn);
        rightDelay_.write(rightShifted);
        auto rightDelayed = rightDelay_.readLinear(rightDelaySamples_);
        rightFeedbackState_ = rightDelayed;
        auto rightStuttered = rightStutter_.process(rightDelayed);

        left = lerp(dryLeft, leftStuttered, leftMix_);
        right = lerp(dryRight, rightStuttered, rightMix_);
    }

  private:
    // A one-shot ramp toward a target cents value, holding once reached;
    // a "random pitch" trigger jumps straight to its target and holds.
    struct Sweep
    {
        float value = 0.0f;
        float target = 0.0f;
        bool rising = false;
        bool active = false;

        void startUp(float maxCents)
        {
            value = 0.0f;
            target = maxCents;
            rising = true;
            active = true;
        }
        void startDown(float minCents)
        {
            value = 0.0f;
            target = minCents;
            rising = false;
            active = true;
        }
        void jumpRandom(float cents)
        {
            value = cents;
            target = cents;
            active = false;
        }
        void advance(float sampleRate, float upSpeed, float dnSpeed)
        {
            if (!active)
            {
                return;
            }
            auto speed = (rising ? upSpeed : dnSpeed) / sampleRate;
            if (rising)
            {
                value = std::min(value + speed, target);
                if (value >= target)
                {
                    active = false;
                }
            }
            else
            {
                value = std::max(value - speed, target);
                if (value <= target)
                {
                    active = false;
                }
            }
        }
    };

    static constexpr float kSweepRateScale = 2400.0f; // cents/second at rate=100

    float nextRandom(float maxCents)
    {
        randomState_ ^= randomState_ << 13;
        randomState_ ^= randomState_ >> 17;
        randomState_ ^= randomState_ << 5;
        auto normalized = static_cast<float>(static_cast<int32_t>(randomState_)) / 2147483648.0f; // -1..1
        return normalized * maxCents;
    }

    void updateAutoSequencer()
    {
        if (!autoEnabled_)
        {
            return;
        }
        if (autoCountdownSamples_ > 0.0f)
        {
            autoCountdownSamples_ -= 1.0f;
            return;
        }
        fireAutoTrigger();
        auto intervalSeconds = lerp(2.0f, 0.05f, speed_ / 100.0f);
        autoCountdownSamples_ = intervalSeconds * sampleRate_;
    }

    void fireAutoTrigger()
    {
        auto pick = static_cast<uint32_t>(std::fabs(nextRandom(1000.0f)));
        switch (program_)
        {
            case Program::kJustStutter:
                (pick % 2 == 0) ? triggerStutter1() : triggerStutter2();
                break;
            case Program::kRandomPitch:
                (pick % 2 == 0) ? triggerRandomPitch1() : triggerRandomPitch2();
                break;
            case Program::kRandomSweep:
                switch (pick % 4)
                {
                    case 0:
                        triggerSweepUp1();
                        break;
                    case 1:
                        triggerSweepDown1();
                        break;
                    case 2:
                        triggerSweepUp2();
                        break;
                    default:
                        triggerSweepDown2();
                        break;
                }
                break;
            case Program::kTotalRandom:
            default:
                switch (pick % 6)
                {
                    case 0:
                        triggerStutter1();
                        break;
                    case 1:
                        triggerStutter2();
                        break;
                    case 2:
                        triggerSweepUp1();
                        break;
                    case 3:
                        triggerSweepDown1();
                        break;
                    case 4:
                        triggerRandomPitch1();
                        break;
                    default:
                        triggerRandomPitch2();
                        break;
                }
                break;
        }
    }

    float sampleRate_ = 48000.0f;

    PitchShifter leftShifter_, rightShifter_;
    DelayLine leftDelay_, rightDelay_;
    StutterCapture leftStutter_, rightStutter_;

    float length1Samples_ = 0.0f, length2Samples_ = 0.0f;
    int count1_ = 0, count2_ = 0;

    float leftBaseCents_ = 0.0f, rightBaseCents_ = 0.0f;
    float leftDelaySamples_ = 0.0f, rightDelaySamples_ = 0.0f;
    float leftFeedback_ = 0.0f, rightFeedback_ = 0.0f;
    float leftFeedbackState_ = 0.0f, rightFeedbackState_ = 0.0f;

    float up1Rate_ = 50.0f, up1Max_ = 100.0f;
    float dn1Rate_ = 50.0f, dn1Min_ = -100.0f;
    float up2Rate_ = 50.0f, up2Max_ = 100.0f;
    float dn2Rate_ = 50.0f, dn2Min_ = -100.0f;
    float rand1Max_ = 1200.0f, rand2Max_ = 1200.0f;
    SweepTarget sweepTarget1_ = SweepTarget::kLeft;
    SweepTarget sweepTarget2_ = SweepTarget::kRight;
    Sweep sweep1_, sweep2_;

    float leftMix_ = 1.0f, rightMix_ = 1.0f;

    bool autoEnabled_ = false;
    float speed_ = 50.0f;
    Program program_ = Program::kTotalRandom;
    float autoCountdownSamples_ = 0.0f;

    uint32_t randomState_ = 0x1234ABCDu;
};
}
