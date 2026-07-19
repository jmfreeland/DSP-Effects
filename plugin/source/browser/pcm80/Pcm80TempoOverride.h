#pragma once

#include "Pcm80Preset.h"

// Lets a PCM80 import use a different tempo than the one baked into the
// preset's own ROM data (its decoded numeric ms value already reflects
// the preset's own stored Tempo Rate - see docs/pcm81-echo-beat-tempo-
// sync.md) - the same override the PCM81 hardware itself offers via a
// master unit tempo setting that supersedes a loaded preset's own value,
// letting a preset's tempo-synced delays track the current session/DAW
// tempo instead.
namespace loom::browser::pcm80
{
// The same numerator/denominator split and formula as decoder.py's
// decode_tempo_value() - see that function's doc comment for the
// derivation and validation. echoes <= 0 or bpm <= 0 (nothing sensible
// to compute) returns 0.
inline double echoBeatToMs(double raw, double bpm)
{
    auto raw10 = static_cast<int>(raw);
    auto echoes = raw10 >> 5;
    auto beats = raw10 & 0x1F;
    if (echoes <= 0 || bpm <= 0.0)
    {
        return 0.0;
    }
    auto beatMs = 60000.0 / bpm;
    return (static_cast<double>(beats) / static_cast<double>(echoes)) * beatMs;
}

// range_decode 35 is Cycl:Beat (tempo-synced LFO Rate - a frequency, not
// a time; no conversion formula exists yet) - see decoder.py's
// CYCL_BEAT_RDS. Every other tempo-active field is Echo:Beat (a
// delay/time parameter), which echoBeatToMs() handles.
inline bool isCyclBeat(int rangeDecode)
{
    return rangeDecode == 35;
}

// Returns a copy of preset with every tempo-active Echo:Beat field's
// numeric value recomputed against bpm instead of the preset's own
// stored tempo. Cycl:Beat fields, and any tempo-active field with no raw
// echoes component (echoes <= 0), are left as the archive decoded them.
inline Preset withTempoOverride(Preset preset, double bpm)
{
    for (auto& f : preset.fields)
    {
        if (!f.tempoActive || isCyclBeat(f.rangeDecode))
        {
            continue;
        }
        auto ms = echoBeatToMs(f.raw, bpm);
        if (ms > 0.0)
        {
            f.numeric = ms;
            f.unit = "ms";
        }
    }
    return preset;
}
}
