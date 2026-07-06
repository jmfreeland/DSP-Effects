#pragma once

#include <array>
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
//
// This is anchored at the scale's tonic: it answers "how many semitones is
// a transposition of N scale steps starting *from the root*", not "from
// whatever note is currently sounding" - true per-note diatonic harmony
// needs the latter, which needs real-time monophonic pitch detection of
// the input (not implemented here - see DiatonicShift.h's class comment).
inline int diatonicSemitones(Scale scale, int scaleDegrees)
{
    const auto& steps = detail::scaleSteps(scale);
    auto octaves = scaleDegrees >= 0 ? scaleDegrees / 7 : -((-scaleDegrees + 6) / 7);
    auto degree = scaleDegrees - octaves * 7;
    return octaves * 12 + steps[static_cast<std::size_t>(degree)];
}
}
