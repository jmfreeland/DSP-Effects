#pragma once

#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

// One decoded field from a PCM80 preset archive (see
// tools/pcm80-import/pcm80lib/decoder.py's "patchable" output) - a raw
// integer plus its Lexicon-documented meaning. Adapters look these up by
// (group, label) - the same pair the archive's own display already uses
// ("RvbDesign", "Size") - rather than by index, since field order is an
// implementation detail of the bitpack, not something an adapter should
// depend on.
namespace loom::browser::pcm80
{
struct Field
{
    juce::String group;
    juce::String label;
    double raw = 0.0;
    bool tempoActive = false;
    // The archive's own unit-tagged numeric interpretation of raw, where
    // one exists (see range_decode.py's NUMERIC_DECODE_FUNCS) - unset for
    // pure enums, named sources, "Off"/mute, and tempo-synced "Cycl:Beat"
    // fields (tempo-synced LFO Rate; no frequency conversion formula
    // derived yet). Tempo-synced "Echo:Beat" fields (delay/time
    // parameters) *are* converted to a real ms value using the preset's
    // own Tempo Rate - see decoder.py's decode_tempo_value().
    // unit is one of: percent, db, db_phase_inverted, hz, ms, ratio,
    // degrees, meters, pan-1to1, bool, raw, bpm, count.
    std::optional<double> numeric;
    juce::String unit;
};

// One preset, as much of tools/pcm80-import's decoded archive as the
// Loom browser plugin's importer needs - deliberately not the full
// archive schema (no soft-row assignments, no patches): those describe
// MIDI/internal modulation routing this first pass doesn't attempt to
// reconstruct, only the preset's own static parameter values.
struct Preset
{
    juce::String name;
    juce::String knobLabel;
    juce::String algorithm; // e.g. "Plate" - matches EngineAdapter::pcm80AlgorithmName()
    int algorithmId = -1;
    bool reliable = false;
    std::vector<Field> fields;

    const Field* find(const juce::String& group, const juce::String& label) const
    {
        for (auto& f : fields)
        {
            if (f.group == group && f.label == label)
            {
                return &f;
            }
        }
        return nullptr;
    }
};
}
