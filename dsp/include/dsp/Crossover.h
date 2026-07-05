#pragma once

#include "dsp/Math.h"
#include "dsp/OnePole.h"

namespace dsp
{
/**
 * Two-band splitter: a single one-pole lowpass produces the low band, and
 * the low band subtracted from the input produces the complementary high
 * band (low + high == input exactly). Used to give a feedback path
 * independent low- and mid/high-frequency decay rates around a crossover
 * frequency (Lexicon's Low Rt / Mid Rt / Crossover controls).
 */
class Crossover
{
  public:
    struct Bands
    {
        float low;
        float high;
    };

    void setFrequency(float cutoffHz, float sampleRate)
    {
        lowpass_.setCoefficient(onePoleLowpassCoefficient(cutoffHz, sampleRate));
    }

    Bands process(float input)
    {
        auto low = lowpass_.process(input);
        return { low, input - low };
    }

    void reset() { lowpass_.reset(); }

  private:
    OnePoleLowpass lowpass_;
};
}
