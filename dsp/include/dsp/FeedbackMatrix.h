#pragma once

#include <array>
#include <cstddef>

namespace dsp
{
/**
 * Lossless Householder reflection mixing matrix for an N-line feedback
 * delay network. Applying I - (2/N)*J (J = all-ones matrix) to the tank's
 * outputs before feeding them back distributes energy densely across all
 * delay lines every pass without amplifying or attenuating total energy,
 * which is what turns a handful of delay lines into a smooth, dense
 * reverb tail instead of N discrete echoes.
 */
template <std::size_t N>
void householderMix(std::array<float, N>& values)
{
    float sum = 0.0f;
    for (auto v : values)
    {
        sum += v;
    }
    auto correction = sum * (2.0f / static_cast<float>(N));
    for (auto& v : values)
    {
        v -= correction;
    }
}
}
