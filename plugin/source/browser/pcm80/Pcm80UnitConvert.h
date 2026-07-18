#pragma once

#include <algorithm>
#include <cmath>

// Small generic engineering-unit conversions shared by every algorithm's
// PCM80 field mapping (see PlateAdapter.h's importPcm80Preset() for the
// first user) - deliberately just arithmetic, no PCM80- or engine-
// specific knowledge, so the same helpers serve all ten algorithms.
namespace loom::browser::pcm80
{
inline float clampf(float v, float lo, float hi)
{
    return std::clamp(v, lo, hi);
}

// dB -> linear gain (e.g. PCM80's Levels/RefLvl/RvbIn/RvbOut fields,
// which this codebase's own engines take as a 0..1 or -1..1 linear
// fraction rather than dB).
inline float dbToLinear(double db)
{
    return static_cast<float>(std::pow(10.0, db / 20.0));
}

inline float percentToFraction(double percent)
{
    return static_cast<float>(percent / 100.0);
}

inline float msToSeconds(double ms)
{
    return static_cast<float>(ms / 1000.0);
}
}
