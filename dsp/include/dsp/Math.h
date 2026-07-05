#pragma once

#include <algorithm>
#include <cmath>

namespace dsp
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clamp01(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

inline float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// Maps a normalized 0..1 parameter to [minValue, maxValue].
inline float mapLinear(float normalized, float minValue, float maxValue)
{
    return lerp(minValue, maxValue, clamp01(normalized));
}

// One-pole smoothing coefficient for a given time constant (seconds) at sampleRate.
inline float timeConstantToCoefficient(float seconds, float sampleRate)
{
    if (seconds <= 0.0f)
    {
        return 0.0f;
    }
    return std::exp(-1.0f / (seconds * sampleRate));
}

// One-pole lowpass feedback coefficient for a given -3dB cutoff frequency.
inline float onePoleLowpassCoefficient(float cutoffHz, float sampleRate)
{
    return std::exp(-kTwoPi * cutoffHz / sampleRate);
}
}
