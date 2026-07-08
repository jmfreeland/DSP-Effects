#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& mBandRvbSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan" },
        { "diffusion", "Diffusion (shared w/ reverb, inside feedback loop)", StageKind::kFeedback,
          "2x Allpass per channel" },
        { "voiceBank", "6-Voice Multiband (3 left / 3 right)", StageKind::kFeedback,
          "own Delay/Level/Pan/Feedback/HiCut/LoCut per voice" },
        { "reverb", "Chamber (fixed, in parallel)", StageKind::kFeedback, "8-line Householder FDN tank + pre-echo" },
        { "output", "Output", StageKind::kOutput, "FX Mix blends multiband vs. reverb; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "diffusion", nullptr },
        { "diffusion", "voiceBank", nullptr },
        { "voiceBank", "diffusion", "filtered voice output * Fbk - re-diffused each pass" },
        { "input", "reverb", "same raw input, in parallel" },
        { "voiceBank", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "M-Band+Rvb",
        "The third of the manual's five 6-Voice algorithms: six independently band-limited delay "
        "voices (own HiCut/LoCut, ~12dB/octave each) run in parallel with a fixed Chamber reverb. "
        "Unlike Chorus+Rvb, this algorithm's feedback re-enters through the shared Diffusion stage "
        "on every repeat - 'filtered echoes that grow more diffuse with each repeat', per the "
        "manual's own description - rather than bypassing it.",
        stages, connections
    };
    return schema;
}
}
