#pragma once

#include "dsp/PitchShifter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>

namespace dsp::algorithms
{
/**
 * Eventide H3000-inspired "Timesqueeze" (Algorithm 113): per that
 * algorithm's own manual page, "used to speed up or slow down
 * pre-recorded program material without altering the pitch. When used
 * in conjunction with a variable-speed audio tape recorder, this
 * algorithm will automatically control the tape machine playback speed
 * and make the necessary pitch correction."
 *
 * The manual's own #0 Time parameter (-87.5%..100%, a percentage change
 * in program length) and #1 Pitch parameter (0.001..2.000, an
 * independent secondary trim ratio) are both genuinely audio-domain: the
 * *tape speed* half of the effect only exists with a physical deck
 * attached via the manual's separate "Tape Machine Interfacing"
 * parameters (out of scope here - no CV/frequency-control hardware
 * output exists on this software's targets), but the *pitch correction*
 * half is exactly what the H3000's own internal pitch shifter does in
 * real time regardless of whether a deck is connected, and that part
 * this Block reproduces directly: Time% is converted to the tape speed
 * ratio it implies (speedRatio = 1 + time/100), and the shifter is
 * driven by the inverse of that ratio (so a tape sped up 2x, which would
 * otherwise raise pitch an octave, gets shifted back down an octave to
 * compensate) multiplied by the independent Pitch trim.
 *
 * No Block Diagram is given on this algorithm's manual page (unlike
 * every other H3000 algorithm in this archive) and its Levels section
 * lists Left/Right In and Out but no Mix parameter at all - both
 * consistent with this being a fully-wet correction utility rather than
 * a blendable effect, so this Block doesn't expose one.
 */
class Timesqueeze
{
  public:
    static constexpr std::size_t kGrainCapacitySamples = static_cast<std::size_t>(0.3f * 96000.0f);
    static constexpr std::size_t requiredWorkingBufferSize() { return 2 * kGrainCapacitySamples; }

    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        leftShifter_.setBuffer(workingBuffer.subspan(0, kGrainCapacitySamples));
        rightShifter_.setBuffer(workingBuffer.subspan(kGrainCapacitySamples, kGrainCapacitySamples));
        leftShifter_.prepare(sampleRate);
        rightShifter_.prepare(sampleRate);

        setTimePercent(0.0f);
        setPitchRatio(1.0f);
        reset();
    }

    // -87.5 to 100.0 per cent: negative = time compression (tape sped
    // up), positive = time expansion (tape slowed down).
    void setTimePercent(float percent)
    {
        timePercent_ = std::clamp(percent, -87.5f, 100.0f);
        updateShiftRatio();
    }

    // 0.001 to 2.000: an independent pitch trim on top of the automatic
    // time-compensation shift. 1.0 = no additional shift.
    void setPitchRatio(float ratio)
    {
        pitchRatio_ = std::clamp(ratio, 0.001f, 2.000f);
        updateShiftRatio();
    }

    void reset()
    {
        leftShifter_.reset();
        rightShifter_.reset();
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
        left = leftShifter_.process(left);
        right = rightShifter_.process(right);
    }

  private:
    void updateShiftRatio()
    {
        auto speedRatio = 1.0f + timePercent_ / 100.0f;
        auto compensatingRatio = 1.0f / speedRatio;
        auto totalRatio = compensatingRatio * pitchRatio_;
        auto semitones = 12.0f * std::log2(totalRatio);
        leftShifter_.setSemitones(semitones);
        rightShifter_.setSemitones(semitones);
    }

    float timePercent_ = 0.0f;
    float pitchRatio_ = 1.0f;

    PitchShifter leftShifter_, rightShifter_;
};
}
