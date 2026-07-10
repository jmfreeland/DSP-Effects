#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& dualInvSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Submixer Sends: Stereo/L=Rvb,R=FX/Mono/L=FX,R=Rvb" },
        { "rvb", "Inverse", StageKind::kFeedback, "own In Lvl / Mix; Duration/Low Slope/Mid Slope/Shape" },
        { "fx", "Dual Shifter (2 voices)", StageKind::kFeedback,
          "own Delay/Pitch/Level/Pan; Fbk into own input, X-Fbk into the other voice's" },
        { "output", "Output", StageKind::kOutput,
          "Submixer Returns: Stereo/Rvb=L,FX=R/Mono/FX=L,Rvb=R; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "rvb", "Sends" },
        { "input", "fx", "Sends" },
        { "fx", "fx", "Fbk (own voice), X-Fbk (other voice)" },
        { "rvb", "output", "Returns (Parallel routing)" },
        { "fx", "output", "Returns (Parallel routing)" },
        { "rvb", "fx", "Routing = Rvb into FX (series)" },
        { "fx", "rvb", "Routing = FX into Rvb (series, reverse)" },
    };
    static const AlgorithmSchema schema = {
        "Dual-Inv",
        "The third of the manual's five true Dual-FX Pitch algorithms (same shape as Dual-Chmb/"
        "Dual-Plt): a Submixer freely arranging a fixed Inverse reverb - envelope slope controlled "
        "through Duration/Low Slope/Mid Slope/Shape rather than RT60 decay, see "
        "docs/lexicon-pcm81-inverse.md - against a 2-voice \"Dual Shifter\" FX block (each voice an "
        "independent Delay->Pitch->Level->Pan chain up to 1.25s, with Fbk/X-Fbk feedback between the "
        "two voices) via three controls - Sends, Returns, and Routing (Parallel / Rvb-into-FX series / "
        "FX-into-Rvb series, which takes precedence over Sends/Returns when active).",
        stages, connections
    };
    return schema;
}
}
