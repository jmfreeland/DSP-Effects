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
    // The range_decode id this field was encoded with (see
    // range_decode.py) - only actually needed by tempo-override logic
    // (Pcm80TempoOverride.h) to tell tempo-active "Echo:Beat" delay/time
    // fields (any rd id) from "Cycl:Beat" LFO-rate fields (rd 35 -
    // decoder.py's CYCL_BEAT_RDS), since those two need different
    // real-value formulas (time vs frequency) and only the former is
    // implemented.
    int rangeDecode = -1;
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
// reconstruct, only the preset's own static parameter values. Every
// algorithm's Levels/DelayTime/Feedback/Panning field groups also carry
// a "Master" field (e.g. Levels Master) alongside the per-voice "VoiceN"
// fields this importer does read - every EngineAdapter::importPcm80Preset()
// reads only the VoiceN fields directly and ignores Master, since the
// MIDI Implementation Details manual documents each Master field's own
// display formatting (a Range Decode id) but never the formula that
// combines it with the per-voice values, so there's nothing published to
// implement against. A preset with non-neutral Masters (anything other
// than Levels +0dB / DelayTime, Feedback, Panning 100%, all four fields'
// own default) will import its raw per-voice values un-scaled. See
// docs/pcm80-master-voice-parameters.md for what's known so far toward
// eventually closing this gap.
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
