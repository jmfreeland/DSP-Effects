#pragma once

#include "dsp/DelayLine.h"
#include "dsp/OnePole.h"

#include <span>

namespace dsp
{
/**
 * A single Karplus-Strong string: a delay line with a feedback path
 * running through a damping lowpass filter, continuously re-tuned by
 * delay length rather than fixed at construction - the repeated "Detail
 * of Voice" unit behind the Eventide H3000's String Modeller (Algorithm
 * 118): "In -> (+) -> Delay -> Filter -> [feedback to the (+) junction]
 * -> Out." See docs/eventide-string-modeller.md.
 *
 * A one-pole lowpass always has unity gain at DC regardless of its
 * cutoff, so at a low damping (dark "Bright") setting the loop's DC
 * component decays purely by the feedback gain - far slower than the
 * tuned pitch itself, which the lowpass does attenuate each pass. Left
 * alone, a plucked string then outlives its own pitch as an inaudible
 * hum decaying into an ever more obviously *un*pitched thump. A small
 * one-zero DC blocker in the loop (a standard fixture of real
 * Karplus-Strong implementations) removes exactly that DC mode without
 * touching the rest of the spectrum.
 */
class StringVoice
{
  public:
    void setBuffer(std::span<float> buffer) { delay_.setBuffer(buffer); }

    // Samples between excitation and the string's fundamental repeating -
    // sampleRate / frequencyHz. Must stay within the buffer's capacity.
    void setDelaySamples(float samples) { delaySamples_ = samples; }

    // Feedback gain around the loop - the string's resonance/sustain amount.
    void setFeedback(float feedback) { feedback_ = feedback; }

    // Damping lowpass coefficient in the feedback path (see
    // dsp::onePoleLowpassCoefficient) - the manual's "Bright" control.
    void setDampingCoefficient(float coefficient) { damping_.setCoefficient(coefficient); }

    float process(float input)
    {
        auto delayed = delay_.readLinear(delaySamples_);
        auto damped = damping_.process(delayed);
        auto blocked = blockDc(damped);
        delay_.write(input + feedback_ * blocked);
        return delayed;
    }

    void reset()
    {
        delay_.reset();
        damping_.reset();
        dcBlockX1_ = 0.0f;
        dcBlockY1_ = 0.0f;
    }

  private:
    static constexpr float kDcBlockR = 0.995f;

    float blockDc(float input)
    {
        auto output = input - dcBlockX1_ + kDcBlockR * dcBlockY1_;
        dcBlockX1_ = input;
        dcBlockY1_ = output;
        return output;
    }

    DelayLine delay_;
    OnePoleLowpass damping_;
    float delaySamples_ = 0.0f;
    float feedback_ = 0.0f;
    float dcBlockX1_ = 0.0f;
    float dcBlockY1_ = 0.0f;
};
}
