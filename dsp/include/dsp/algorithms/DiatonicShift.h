#pragma once

#include "dsp/DelayLine.h"
#include "dsp/DiatonicScale.h"
#include "dsp/Math.h"
#include "dsp/PitchShifter.h"

#include <algorithm>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * An Eventide H3000-inspired "Diatonic Shift": the box's own default
 * factory program (see docs/eventide-h3000-notes.md) and, per that
 * manual's own framing, about as clear a statement as exists that
 * diatonic-aware pitch shifting is the H3000's signature effect.
 *
 * Stereo dual-mono: independent left/right delay+PitchShifter chains (see
 * PitchShifter.h) run the same Key/Scale/Degree/Regen/Delay settings. The
 * signal loops through an explicit Delay before each pass through the
 * shifter - not just the shifter's own internal grain buffering - so
 * successive repeats are audibly spaced apart in time rather than piling
 * up within a single sample; each lap through the loop adds another
 * diatonic step on top of the last, producing the classic ascending/
 * descending arpeggio cascade this effect is known for. (An earlier
 * version of this fed the shifter's output back into itself with no
 * delay at all, which decayed to silence within a few milliseconds
 * instead of producing audible spaced-out repeats - caught by the host
 * harness's tone-burst render showing the tail dying by t=1s instead of
 * ringing as a cascade.)
 *
 * Known simplification, stated up front rather than glossed over: true
 * H3000-style diatonic harmonization tracks the *currently playing note*
 * and computes the correct interval for that specific scale degree (a
 * major 3rd above scale-degree 1 is 4 semitones; above scale-degree 2 it's
 * only 3) - which needs real-time monophonic pitch detection of the
 * input, not implemented here. This engine instead computes the shift as
 * a fixed transposition of N diatonic scale steps anchored at the scale's
 * tonic (see dsp/DiatonicScale.h), which is musically coherent and stays
 * on-scale whenever the shift amount changes, but won't re-derive a
 * different interval size per input note the way the real hardware does.
 */
class DiatonicShift
{
  public:
    // 1.5s matches the H3000's own documented max delay memory capacity
    // (docs/eventide-h3000-notes.md).
    static constexpr float kMaxDelaySeconds = 1.5f;
    static constexpr float kMaxSampleRate = 96000.0f;
    static constexpr std::size_t kDelayCapacitySamples =
      static_cast<std::size_t>(kMaxDelaySeconds * kMaxSampleRate);
    static constexpr std::size_t kShifterCapacitySamples =
      static_cast<std::size_t>(PitchShifter::kMaxGrainSeconds * kMaxSampleRate);

    static constexpr std::size_t requiredWorkingBufferSize()
    {
        return 2 * (kDelayCapacitySamples + kShifterCapacitySamples);
    }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        sampleRate_ = sampleRate;
        auto half = workingBuffer.size() / 2;
        auto leftBuf = workingBuffer.first(half);
        auto rightBuf = workingBuffer.subspan(half);

        delayLeft_.setBuffer(leftBuf.first(kDelayCapacitySamples));
        shifterLeft_.setBuffer(leftBuf.subspan(kDelayCapacitySamples));
        delayRight_.setBuffer(rightBuf.first(kDelayCapacitySamples));
        shifterRight_.setBuffer(rightBuf.subspan(kDelayCapacitySamples));

        shifterLeft_.prepare(sampleRate);
        shifterRight_.prepare(sampleRate);

        setGrainSeconds(0.07f);
        setDelaySeconds(0.4f);
        setKey(0);
        setScale(Scale::kMajor);
        setScaleDegree(2);
        setRegen(0.5f);
        setMix(0.5f);
        reset();
    }

    // 10-300ms, see PitchShifter::setGrainSeconds.
    void setGrainSeconds(float seconds)
    {
        shifterLeft_.setGrainSeconds(seconds);
        shifterRight_.setGrainSeconds(seconds);
    }

    // 0..1.5s: spacing between successive shifted repeats.
    void setDelaySeconds(float seconds)
    {
        delaySamples_ =
          std::clamp(seconds * sampleRate_, 0.0f, static_cast<float>(kDelayCapacitySamples - 2));
    }

    // 0 (C) .. 11 (B) - not used directly by the shift math (which is
    // anchored at the scale's own tonic, see class comment) but kept as
    // a first-class control since real diatonic tracking - a future
    // upgrade - would need it to know which note the scale is built on.
    void setKey(int key) { key_ = key; }

    void setScale(Scale scale)
    {
        scale_ = scale;
        updateShift();
    }

    // How many diatonic scale steps to transpose per repeat (may be
    // negative, or exceed +/-7 to span multiple octaves).
    void setScaleDegree(int degrees)
    {
        scaleDegrees_ = degrees;
        updateShift();
    }

    // 0..1: feedback level for the cascading repeat/arpeggio - how much of
    // each shifted repeat feeds back in for another lap.
    void setRegen(float amount) { regen_ = std::clamp(amount, 0.0f, 0.97f); }

    // 0 (dry) .. 1 (fully shifted).
    void setMix(float wet) { mix_ = clamp01(wet); }

    void reset()
    {
        delayLeft_.reset();
        delayRight_.reset();
        shifterLeft_.reset();
        shifterRight_.reset();
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

        auto tapLeft = delayLeft_.readLinear(delaySamples_);
        auto shiftedLeft = shifterLeft_.process(tapLeft);
        delayLeft_.write(dryLeft + regen_ * shiftedLeft);

        auto tapRight = delayRight_.readLinear(delaySamples_);
        auto shiftedRight = shifterRight_.process(tapRight);
        delayRight_.write(dryRight + regen_ * shiftedRight);

        left = lerp(dryLeft, shiftedLeft, mix_);
        right = lerp(dryRight, shiftedRight, mix_);
    }

  private:
    void updateShift()
    {
        auto semitones = static_cast<float>(diatonicSemitones(scale_, scaleDegrees_));
        shifterLeft_.setSemitones(semitones);
        shifterRight_.setSemitones(semitones);
    }

    DelayLine delayLeft_;
    DelayLine delayRight_;
    PitchShifter shifterLeft_;
    PitchShifter shifterRight_;
    float sampleRate_ = 48000.0f;
    float delaySamples_ = 0.0f;
    int key_ = 0;
    Scale scale_ = Scale::kMajor;
    int scaleDegrees_ = 2;
    float regen_ = 0.5f;
    float mix_ = 0.5f;
};
}
