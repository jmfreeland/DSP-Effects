#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& quadHallSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan" },
        { "leftVoices", "Voices 1-2 (Left)", StageKind::kFeedback,
          "own Delay/Pitch/Level/Pan; Fbk into own bus, X-Fbk into the right bus" },
        { "rightVoices", "Voices 3-4 (Right)", StageKind::kFeedback,
          "own Delay/Pitch/Level/Pan; Fbk into own bus, X-Fbk into the left bus" },
        { "reverb", "Concert Hall (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank" },
        { "output", "Output", StageKind::kOutput, "FX Mix blends dry-shifted vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "leftVoices", "Left bus" },
        { "input", "rightVoices", "Right bus" },
        { "leftVoices", "leftVoices", "Fbk (own bus)" },
        { "rightVoices", "rightVoices", "Fbk (own bus)" },
        { "leftVoices", "rightVoices", "X-Fbk" },
        { "rightVoices", "leftVoices", "X-Fbk" },
        { "leftVoices", "reverb", "series - the shifted signal becomes the reverb's input" },
        { "rightVoices", "reverb", nullptr },
        { "leftVoices", "output", "FX Mix = 0" },
        { "rightVoices", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "Quad>Hall",
        "The first of the manual's seven Pitch algorithms, and the only one without a Submixer: a "
        "4-voice pitch shifter (Voices 1-2 fed from the Left input, Voices 3-4 from the Right, each "
        "an independent Delay->Pitch->Level->Pan chain up to 1.25s) with per-voice feedback and "
        "cross-feedback between the two channel buses, wired in fixed series with a Concert Hall "
        "reverb - \"the reverb effect is fixed in position following the pitch shifters, with a "
        "final Mix control allowing control over the amount of reverb in the processed sound.\"",
        stages, connections
    };
    return schema;
}
}
