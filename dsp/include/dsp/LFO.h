#pragma once

#include "dsp/Math.h"

#include <cmath>
#include <cstdint>

namespace dsp
{
/**
 * Simple free-running low-frequency oscillator used to wobble delay lengths
 * (chorus-style modulation inside a reverb tank, pitch-shifter crossfade
 * timing, etc). No allocation, trivially cheap per sample.
 */
class LFO
{
  public:
    void setFrequency(float hz, float sampleRate)
    {
        increment_ = hz / sampleRate;
    }

    void setPhase(float phase01) { phase_ = phase01; }

    // Reseeds nextRandomWalk()'s own xorshift generator. Needed because
    // every LFO instance otherwise starts from the same hardcoded seed
    // (see rngState_'s default below): nextRandomBipolar() is a pure
    // function of that state, so two default-constructed LFOs draw the
    // exact same *sequence of walk targets* regardless of Rate or
    // setPhase() - only the timing at which each instance reaches its
    // Nth target differs, not the target's value. Confirmed directly (a
    // 0.4Hz and a 4.7Hz instance's end-of-cycle values matched to ~2
    // decimal places for 5+ consecutive cycles despite the very
    // different rates). Callers driving several LFOs whose
    // nextRandomWalk() outputs get summed together (Swept Combs' six
    // lines, Swept Reverb's own sweepLfo_ array) need a distinct seed
    // per instance or the "independent" random sweeps are actually the
    // same sequence just sampled at different speeds. 0 is rejected
    // (xorshift32's fixed point) and mapped to the default seed instead.
    void setSeed(std::uint32_t seed) { rngState_ = seed != 0 ? seed : 0x9e3779b9u; }

    float nextSine()
    {
        auto value = std::sin(phase_ * kTwoPi);
        advance();
        return value;
    }

    float nextTriangle()
    {
        // Triangle in [-1, 1] from a 0..1 phase ramp.
        auto value = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;
        advance();
        return value;
    }

    // A smoothly-wandering random source in [-1, 1]: picks a new random
    // target once per cycle (at setFrequency()'s rate) and linearly
    // slides toward it, rather than jumping - the Eventide H3000 Swept
    // Combs/Swept Reverb sweep generator's own description: "Is it a
    // sine, ramp or triangle sweep? Actually, it's not any of those...
    // The algorithm uses random numbers to achieve a more complex and
    // thicker texture." A self-contained xorshift PRNG keeps this
    // allocation-free and independent of libc's global rand() state.
    float nextRandomWalk()
    {
        if (phase_ < previousPhase_)
        {
            walkFrom_ = walkTo_;
            walkTo_ = nextRandomBipolar();
        }
        previousPhase_ = phase_;
        auto value = lerp(walkFrom_, walkTo_, phase_);
        advance();
        return value;
    }

  private:
    void advance()
    {
        phase_ += increment_;
        if (phase_ >= 1.0f)
        {
            phase_ -= 1.0f;
        }
    }

    float nextRandomBipolar()
    {
        // xorshift32.
        rngState_ ^= rngState_ << 13;
        rngState_ ^= rngState_ >> 17;
        rngState_ ^= rngState_ << 5;
        return (static_cast<float>(rngState_) / 4294967295.0f) * 2.0f - 1.0f;
    }

    float phase_ = 0.0f;
    float previousPhase_ = 0.0f;
    float increment_ = 0.0f;
    float walkFrom_ = 0.0f;
    float walkTo_ = 0.0f;
    std::uint32_t rngState_ = 0x9e3779b9u;
};
}
