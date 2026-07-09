#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& res1PlateSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan, Voice Diffusion" },
        { "resonators", "6 Resonator Voices (3 left / 3 right)", StageKind::kFeedback,
          "own Pitch/Level/Pan/Duration/HiCut per voice, StringVoice Karplus-Strong resonance" },
        { "reverb", "Plate (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank" },
        { "output", "Output", StageKind::kOutput, "FX Mix blends pre-reverb vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "resonators", nullptr },
        { "resonators", "resonators", "continuously excited by the live input, not a plucked trigger" },
        { "resonators", "reverb", "series - the six resonators' output becomes the reverb's input" },
        { "resonators", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "Res1>Plate",
        "The fourth of the manual's five 6-Voice algorithms: six Karplus-Strong-style resonator "
        "voices (three excited by the left input, three by the right, each with its own directly "
        "settable Pitch/Level/Pan/Duration/HiCut), whose combined output runs in series into a "
        "fixed Plate reverb. The manual assigns resonator pitch chromatically via a MIDI note "
        "round-robin; since this project has no MIDI input pathway, pitch is instead a direct "
        "per-voice control - see Res2>Plate for the diatonic-harmony sibling.",
        stages, connections
    };
    return schema;
}
}
