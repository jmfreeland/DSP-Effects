#pragma once

#include <algorithm>
#include <cmath>

namespace dsp
{
// Feedback gain that decays a signal recirculating every `delaySamples`
// down to -60dB after `rt60Seconds`. Used to give feedback delay lines a
// controllable, physically-meaningful decay time regardless of length.
inline float rt60ToGain(float delaySamples, float sampleRate, float rt60Seconds)
{
    auto delaySeconds = delaySamples / sampleRate;
    auto clamped = std::max(rt60Seconds, 0.001f);
    return std::pow(10.0f, -3.0f * delaySeconds / clamped);
}
}
