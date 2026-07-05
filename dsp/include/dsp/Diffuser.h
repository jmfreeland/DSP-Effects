#pragma once

#include "dsp/Allpass.h"
#include "dsp/Math.h"

#include <array>
#include <cstddef>
#include <span>

namespace dsp
{
/**
 * A chain of N series allpass filters sharing a single Diffusion amount,
 * turning a transient into a dense cluster of echoes before it reaches a
 * reverb tank. This is the "Diffusion" block common to every reverb-core
 * diagram (Concert Hall, Plate, Chamber, Inverse, Infinite all have one).
 */
template <std::size_t N>
class DiffuserChain
{
  public:
    void setStageBuffer(std::size_t index, std::span<float> buffer)
    {
        stages_[index].setBuffer(buffer);
    }

    // amount in [0, 1]; maps to the allpass coefficient range typical for
    // diffusion (0 = no diffusion, 1 = maximum density/smear).
    void setDiffusion(float amount)
    {
        auto coefficient = mapLinear(amount, 0.0f, 0.75f);
        for (auto& stage : stages_)
        {
            stage.setCoefficient(coefficient);
        }
    }

    float process(float input)
    {
        auto x = input;
        for (auto& stage : stages_)
        {
            x = stage.process(x);
        }
        return x;
    }

    void reset()
    {
        for (auto& stage : stages_)
        {
            stage.reset();
        }
    }

  private:
    std::array<Allpass, N> stages_;
};
}
