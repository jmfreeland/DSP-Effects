#pragma once

#include "dsp/algorithms/Chamber.h"

#include <span>

namespace dsp::algorithms
{
/**
 * A Lexicon PCM81-inspired "Infinite": Chamber plus a freeze switch - the
 * manual describes it as literally "Chamber + a freeze switch," with the
 * tail ringing forever and the reverb input ramping off while frozen. The
 * freeze mechanism (setFrozen(), gating new input out of the tank and
 * pushing the tank's own decay gains to near-lossless) already exists
 * generically on ReverbCore - it was added early for Concert Hall's
 * footswitch-hold behavior, and this algorithm is the manual's own
 * confirmation that a shared, generic freeze belongs on the base class
 * rather than being Infinite-specific. So Infinite adds nothing of its
 * own beyond Chamber's Shape/Spread/pre-echo - just different default
 * tuning (a longer decay, since Infinite's whole character is about a
 * long-lived tail before it's ever frozen).
 *
 * Known simplifications:
 *  - "reverb input ramps off" in the manual implies a smooth fade;
 *    ReverbCore's freeze currently gates the diffuser input instantly
 *    (the same simplification Concert Hall's freeze has always had)
 *    rather than ramping it, so toggling freeze can click faintly.
 *  - Building this algorithm is what surfaced (and fixed) a real bug in
 *    ReverbCore's freeze: the Rt HC damping filter was still eroding the
 *    tail every sample even while frozen, so a "frozen" tail measurably
 *    decayed over a few seconds instead of holding - see the frozen_
 *    branch around the damping filter in ReverbCore::processSample().
 *    Even after that fix, Spin/Chorus's continuous delay-length
 *    modulation costs a small amount of energy per pass (linear
 *    interpolation between wobbling read positions is a lossy
 *    operation) - a genuinely infinite hold wants Spin/Chorus rolled
 *    back toward 0, which isn't enforced automatically.
 */
class Infinite : public Chamber
{
  public:
    void prepare(float sampleRate, std::span<float> workingBuffer)
    {
        Chamber::prepare(sampleRate, workingBuffer);
        setDecaySeconds(6.0f);
        setDiffusion(0.5f);
        setShape(0.2f);
        setSpread(0.6f);
        reset();
    }
};
}
