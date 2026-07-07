#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace dsp
{
// A handful of common 7-note scales, as semitone offsets from the tonic.
enum class Scale
{
    kMajor,
    kNaturalMinor,
    kHarmonicMinor,
    kDorian,
    kMixolydian,
};

namespace detail
{
inline const std::array<int, 7>& scaleSteps(Scale scale)
{
    static constexpr std::array<int, 7> kMajor = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr std::array<int, 7> kNaturalMinor = { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr std::array<int, 7> kHarmonicMinor = { 0, 2, 3, 5, 7, 8, 11 };
    static constexpr std::array<int, 7> kDorian = { 0, 2, 3, 5, 7, 9, 10 };
    static constexpr std::array<int, 7> kMixolydian = { 0, 2, 4, 5, 7, 9, 10 };
    switch (scale)
    {
        case Scale::kNaturalMinor:
            return kNaturalMinor;
        case Scale::kHarmonicMinor:
            return kHarmonicMinor;
        case Scale::kDorian:
            return kDorian;
        case Scale::kMixolydian:
            return kMixolydian;
        case Scale::kMajor:
        default:
            return kMajor;
    }
}
}

// Semitone offset (from the tonic) for transposing up scaleDegrees diatonic
// steps in the given scale - e.g. diatonicSemitones(kMajor, 2) == 4 (a
// major 3rd up from the root). scaleDegrees may be negative or exceed
// +/-7, wrapping through as many octaves as needed.
inline int diatonicSemitones(Scale scale, int scaleDegrees)
{
    const auto& steps = detail::scaleSteps(scale);
    auto octaves = scaleDegrees >= 0 ? scaleDegrees / 7 : -((-scaleDegrees + 6) / 7);
    auto degree = scaleDegrees - octaves * 7;
    return octaves * 12 + steps[static_cast<std::size_t>(degree)];
}

// The absolute scale-degree index (may span octaves, like diatonicSemitones'
// own scaleDegrees parameter) of whichever scale tone is closest to a given
// continuous semitone offset from the tonic - e.g. if semitoneFromTonic
// lands almost exactly on the major 3rd, this returns 2. Used to figure out
// "which diatonic step is the currently-playing note actually on" from a
// real-time pitch detector's continuous frequency reading, so a harmony
// interval (see HarmonicInterval below) can be applied relative to *that*
// degree rather than a fixed assumption - see DiatonicShift.h.
inline int nearestScaleDegreeIndex(Scale scale, float semitoneFromTonic)
{
    const auto& steps = detail::scaleSteps(scale);
    auto octave = static_cast<int>(std::floor(semitoneFromTonic / 12.0f));
    auto pitchClass = semitoneFromTonic - static_cast<float>(octave) * 12.0f; // [0, 12)

    auto bestDegree = 0;
    auto bestDistance = std::fabs(pitchClass - static_cast<float>(steps[0]));
    for (int degree = 1; degree < 7; ++degree)
    {
        auto distance = std::fabs(pitchClass - static_cast<float>(steps[static_cast<std::size_t>(degree)]));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestDegree = degree;
        }
    }
    // The next octave's tonic (degree 7, i.e. this octave + 1's degree 0)
    // may be closer than any in-octave degree for pitch classes near 12.
    auto distanceToNextTonic = std::fabs(pitchClass - 12.0f);
    if (distanceToNextTonic < bestDistance)
    {
        bestDegree = 7;
    }
    return octave * 7 + bestDegree;
}

// The discrete harmony-interval choices the H3000's Diatonic Shift offers
// per voice (docs/references excerpt, "Algorithm 100 - Diatonic Shift" -
// see docs/eventide-diatonic-shift.md), minus the two fully-custom
// Scale 1/Scale 2 per-note tables (a documented remaining gap - those are
// arbitrary cents-per-chromatic-note tables, not diatonic-degree offsets,
// and don't fit this enum's model).
enum class HarmonicInterval
{
    kOctaveDown,
    kSeventhDown,
    kSixthDown,
    kFifthDown,
    kFourthDown,
    kThirdDown,
    kSecondDown,
    kSecondUp,
    kThirdUp,
    kFourthUp,
    kFifthUp,
    kSixthUp,
    kSeventhUp,
    kOctaveUp,
    kLowTonicPedal,
    kHighTonicPedal,
    kLowDominantPedal,
    kHighDominantPedal,
};

// Diatonic scale-degree offset for the relative (non-pedal) intervals -
// e.g. kThirdUp is +2 scale steps. Pedal-tone intervals aren't relative
// offsets (they target an absolute scale degree - the tonic or the
// dominant/5th - regardless of the input's own degree), so they aren't
// meaningful here; DiatonicShift.h handles them separately.
inline int harmonicIntervalDegreeOffset(HarmonicInterval interval)
{
    switch (interval)
    {
        case HarmonicInterval::kOctaveDown:
            return -7;
        case HarmonicInterval::kSeventhDown:
            return -6;
        case HarmonicInterval::kSixthDown:
            return -5;
        case HarmonicInterval::kFifthDown:
            return -4;
        case HarmonicInterval::kFourthDown:
            return -3;
        case HarmonicInterval::kThirdDown:
            return -2;
        case HarmonicInterval::kSecondDown:
            return -1;
        case HarmonicInterval::kSecondUp:
            return 1;
        case HarmonicInterval::kThirdUp:
            return 2;
        case HarmonicInterval::kFourthUp:
            return 3;
        case HarmonicInterval::kFifthUp:
            return 4;
        case HarmonicInterval::kSixthUp:
            return 5;
        case HarmonicInterval::kSeventhUp:
            return 6;
        case HarmonicInterval::kOctaveUp:
            return 7;
        default:
            return 0; // pedal tones: handled separately, see above.
    }
}

// Inverse of harmonicIntervalDegreeOffset(), for callers that want a
// simple continuous integer control (e.g. a 3-knob hardware Patch)
// rather than the full named interval list. offset is clamped to
// [-7, 7] and 0 is treated as +1 (a plain relative "unison" isn't one
// of the manual's own choices either - see the interval list above,
// which has no zero-offset entry).
inline HarmonicInterval harmonicIntervalFromDegreeOffset(int offset)
{
    if (offset == 0)
    {
        offset = 1;
    }
    offset = offset < -7 ? -7 : (offset > 7 ? 7 : offset);
    static constexpr HarmonicInterval kByOffset[15] = {
        HarmonicInterval::kOctaveDown,  HarmonicInterval::kSeventhDown, HarmonicInterval::kSixthDown,
        HarmonicInterval::kFifthDown,   HarmonicInterval::kFourthDown,  HarmonicInterval::kThirdDown,
        HarmonicInterval::kSecondDown,  HarmonicInterval::kSecondUp, // index 7 is offset 0, unused
        HarmonicInterval::kSecondUp,    HarmonicInterval::kThirdUp,     HarmonicInterval::kFourthUp,
        HarmonicInterval::kFifthUp,     HarmonicInterval::kSixthUp,     HarmonicInterval::kSeventhUp,
        HarmonicInterval::kOctaveUp,
    };
    return kByOffset[static_cast<std::size_t>(offset + 7)];
}
}
