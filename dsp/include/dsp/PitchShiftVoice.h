#pragma once

#include "dsp/DelayLine.h"
#include "dsp/PitchShifter.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp
{
/**
 * A Delay feeding a PitchShifter, one input in, one shifted output out -
 * the "Left Shift"/"Right Shift" unit that recurs across most of the
 * Eventide H3000's pitch-shift algorithms (Layered Shift, Dual Shift,
 * Stereo Shift, Multi-Shift; see docs/eventide-h3000-notes.md). Unlike
 * dsp::algorithms::DiatonicShift's Voice usage - where the shift amount
 * comes from a shared PitchDetector plus diatonic scale math - this
 * Component takes a direct semitone/cents shift amount, matching those
 * algorithms' own "Coarse, Fine" cents parameter (no pitch tracking
 * involved). Each owning Block decides its own input/feedback wiring
 * (shared input point, fully independent per channel, etc. - these differ
 * per algorithm per the manual) and its own Mix; this Component is
 * intentionally just the "one input, one shifted output" unit, not a full
 * effect.
 */
class PitchShiftVoice
{
  public:
    static constexpr float kDefaultMaxSampleRate = 96000.0f;

    static constexpr std::size_t delayCapacitySamples(float maxDelaySeconds,
                                                        float maxSampleRate = kDefaultMaxSampleRate)
    {
        return static_cast<std::size_t>(maxDelaySeconds * maxSampleRate);
    }

    static constexpr std::size_t shifterCapacitySamples(float maxSampleRate = kDefaultMaxSampleRate)
    {
        return static_cast<std::size_t>(PitchShifter::kMaxGrainSeconds * maxSampleRate);
    }

    static constexpr std::size_t requiredWorkingBufferSize(float maxDelaySeconds,
                                                             float maxSampleRate = kDefaultMaxSampleRate)
    {
        return delayCapacitySamples(maxDelaySeconds, maxSampleRate) + shifterCapacitySamples(maxSampleRate);
    }

    // maxDelaySeconds bounds both the working-buffer split above and this
    // instance's own setDelaySeconds() clamp - pass the same value to both.
    void prepare(float sampleRate, std::span<float> workingBuffer, float maxDelaySeconds)
    {
        sampleRate_ = sampleRate;
        maxDelaySamples_ = static_cast<float>(delayCapacitySamples(maxDelaySeconds, kDefaultMaxSampleRate)) - 2.0f;
        auto delayCapacity = delayCapacitySamples(maxDelaySeconds, kDefaultMaxSampleRate);
        delay_.setBuffer(workingBuffer.subspan(0, delayCapacity));
        shifter_.setBuffer(workingBuffer.subspan(delayCapacity));
        shifter_.prepare(sampleRate);
        reset();
    }

    // 10ms..300ms, see PitchShifter::setGrainSeconds.
    void setGrainSeconds(float seconds) { shifter_.setGrainSeconds(seconds); }

    // Time between this voice's input and its shifted output (also what
    // the shifter reads from, so this doubles as its "processing time").
    void setDelaySeconds(float seconds)
    {
        delaySamples_ = std::clamp(seconds * sampleRate_, 0.0f, maxDelaySamples_);
    }

    // Direct pitch shift, no tracking - matches the manual's "Coarse,
    // Fine" cents parameter (caller converts cents to semitones: /100).
    void setSemitones(float semitones) { shifter_.setSemitones(semitones); }

    void reset()
    {
        delay_.reset();
        shifter_.reset();
    }

    float process(float input)
    {
        delay_.write(input);
        auto delayed = delay_.readLinear(delaySamples_);
        return shifter_.process(delayed);
    }

  private:
    DelayLine delay_;
    PitchShifter shifter_;
    float sampleRate_ = 48000.0f;
    float delaySamples_ = 0.0f;
    float maxDelaySamples_ = 0.0f;
};
}
