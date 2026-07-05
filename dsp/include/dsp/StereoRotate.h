#pragma once

#include "dsp/Math.h"

#include <cmath>

namespace dsp
{
/**
 * Rotates a stereo pair in mid/side space by a continuously-variable
 * angle. At 0 degrees the signal passes through unchanged; the angle
 * cycles the image through progressively wider, then swapped, then
 * inverted states as it sweeps toward +/-180 degrees, repeating every
 * 360 degrees - the same periodic, continuously-variable character
 * described for the PCM81's Width controls (a single knob sweeping
 * through narrow/normal/wide/surround/inverted stereo images).
 *
 * This is an original reconstruction of that *behavior* (periodic,
 * continuously variable, several named landmark states per cycle), not a
 * verified match to Lexicon's own internal transfer function or its
 * specific labeled value table.
 */
inline void rotateStereoWidth(float& left, float& right, float degrees)
{
    auto mid = 0.5f * (left + right);
    auto side = 0.5f * (left - right);

    auto radians = degrees * (kPi / 180.0f);
    auto c = std::cos(radians);
    auto s = std::sin(radians);

    auto newMid = mid * c - side * s;
    auto newSide = mid * s + side * c;

    left = newMid + newSide;
    right = newMid - newSide;
}
}
