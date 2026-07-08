#pragma once

#include "dsp/Math.h"

#include <algorithm>
#include <cmath>

namespace dsp
{
/**
 * An attack/decay envelope follower with a second, compressor-style
 * "ducker" output alongside its plain envelope output - the "Envelope
 * Detector" module in the Eventide H3000's mod factory algorithms
 * (122/123, see docs/eventide-mod-factory-one.md). Per that module's own
 * description: "The envelope output is what you would expect, a signal
 * that varies in proportion to the input... The ducker output is... at a
 * high level [normally]. When the input... exceeds a threshold, the
 * output gets progressively smaller" - i.e. a gain-reduction control
 * signal meant to be patched into another module (typically an
 * Amplitude Modulator) so one signal can duck another out of the way,
 * exactly like a compressor's gain computer.
 */
class EnvelopeDucker
{
  public:
    void prepare(float sampleRate) { sampleRate_ = sampleRate; }

    // 0..1000ms: how fast both outputs respond to a rising input level.
    void setAttackMs(float ms) { attackCoefficient_ = timeConstantToCoefficient(std::max(ms, 0.0f) / 1000.0f, sampleRate_); }
    // 0..1000ms: how fast both outputs respond to a falling input level.
    void setDecayMs(float ms) { decayCoefficient_ = timeConstantToCoefficient(std::max(ms, 0.0f) / 1000.0f, sampleRate_); }
    // 0 to -40dB: the level (converted to linear amplitude) above which
    // the ducker output starts reducing.
    void setThresholdDb(float db) { thresholdLinear_ = std::pow(10.0f, std::min(db, 0.0f) / 20.0f); }
    // 1:1 to 100:1, a compressor-style ratio applied above threshold.
    void setRatio(float ratioToOne) { ratio_ = std::max(ratioToOne, 1.0f); }

    void reset() { envelopeLevel_ = 0.0f; }

    struct Outputs
    {
        float envelope; // proportional to input level, 0..~1
        float ducker;   // starts near 1 (fullscale), shrinks above threshold
    };

    Outputs process(float input)
    {
        auto rectified = std::fabs(input);
        auto coefficient = rectified > envelopeLevel_ ? attackCoefficient_ : decayCoefficient_;
        envelopeLevel_ = rectified + (envelopeLevel_ - rectified) * coefficient;

        auto ducker = 1.0f;
        if (envelopeLevel_ > thresholdLinear_ && thresholdLinear_ > 0.0f)
        {
            // Compressor-style gain computer in the dB domain: the amount
            // above threshold is reduced by `ratio_`, then converted back
            // to a linear gain that shrinks as the input grows.
            auto aboveDb = 20.0f * std::log10(envelopeLevel_ / thresholdLinear_);
            auto reducedDb = aboveDb / ratio_;
            ducker = std::pow(10.0f, (reducedDb - aboveDb) / 20.0f);
        }

        return { envelopeLevel_, ducker };
    }

  private:
    float sampleRate_ = 48000.0f;
    float attackCoefficient_ = 0.0f;
    float decayCoefficient_ = 0.0f;
    float thresholdLinear_ = 1.0f;
    float ratio_ = 1.0f;
    float envelopeLevel_ = 0.0f;
};
}
