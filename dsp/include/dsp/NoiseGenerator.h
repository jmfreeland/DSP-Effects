#pragma once

#include <cstdint>

namespace dsp
{
/**
 * Deterministic white noise source (xorshift32 PRNG mapped to -1..1), the
 * "Noise Gen" block in the Eventide H3000's Patch Factory algorithm
 * (#111) - a fixed seed keeps host-render/plugin output reproducible
 * rather than depending on wall-clock entropy.
 */
class NoiseGenerator
{
  public:
    float next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        constexpr float kInvMax = 1.0f / 2147483648.0f;
        return static_cast<float>(static_cast<int32_t>(state_)) * kInvMax;
    }

    void reset() { state_ = kSeed; }

  private:
    static constexpr uint32_t kSeed = 0x9E3779B9u;
    uint32_t state_ = kSeed;
};
}
