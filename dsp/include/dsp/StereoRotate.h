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

/**
 * Same rotation as rotateStereoWidth(), re-phased to match the specific
 * degree convention the PCM81's own MIDI Implementation Details manual
 * documents for its Width controls' display value: 0 degrees = "MONO",
 * 45 = "STEREO" (the normal, undistorted image - NOT 0, despite
 * rotateStereoWidth()'s own 0-degrees-is-identity convention), 90 =
 * "L-R, R-L", continuing through swapped and inverted landmarks every 45
 * degrees to a full period at 360. A real preset's decoded FX/Rvb Width
 * value is exactly this Lexicon-native degrees figure, so PCM81-family
 * Graphs should feed it through this wrapper rather than
 * rotateStereoWidth() directly - discovered because doing the latter
 * turned a decoded "+45" (intended as a near-default, normal-stereo
 * value) into a ~4x L/R loudness collapse instead. rotateStereoWidth()
 * itself is left with its own plain 0-is-identity convention because
 * other callers (BandDelay/MultiShift's Eventide-side stereo image
 * controls) already rely on exactly that, unrelated to this PCM81
 * display convention.
 *
 * Like rotateStereoWidth() itself, this can't exactly reproduce every
 * named landmark (a pure two-variable mid/side rotation can't reach a
 * literal, input-independent L=R "MONO" state - no rotation can null out
 * one axis for every possible input), but it does put the one landmark
 * real presets actually use - "STEREO", i.e. no change - at the same
 * degrees value Lexicon's own manual displays for it.
 */
inline void rotatePcm81Width(float& left, float& right, float lexiconDegrees)
{
    rotateStereoWidth(left, right, lexiconDegrees - 45.0f);
}
}
