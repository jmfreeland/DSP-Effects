#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& chorusRvbSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan" },
        { "diffusion", "Diffusion (shared w/ reverb)", StageKind::kProcessing, "2x Allpass per channel" },
        { "highCut", "Chorus High Cut", StageKind::kProcessing, "one-pole, Controls row" },
        { "voiceBank", "6-Voice Chorus (3 left / 3 right)", StageKind::kFeedback,
          "own Delay/Level/Pan/Feedback/Depth/Rate per voice" },
        { "reverb", "Plate (fixed, in parallel)", StageKind::kFeedback, "8-line Householder FDN tank + pre-echo" },
        { "output", "Output", StageKind::kOutput, "FX Mix blends chorus vs. reverb; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "diffusion", nullptr },
        { "diffusion", "highCut", nullptr },
        { "highCut", "voiceBank", nullptr },
        { "voiceBank", "voiceBank", "own-channel Fbk only (no cross-feedback)" },
        { "input", "reverb", "same diffused input, in parallel" },
        { "voiceBank", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "Chorus+Rvb",
        "The second of the manual's five 6-Voice algorithms: a 6-voice stereo chorus (each voice "
        "an independently-modulated delay tap, 3 fed from the left input, 3 from the right) runs "
        "in parallel with a fixed Plate reverb - both paths share the same Diffusion control and "
        "the same raw input, unlike Glide>Hall's series topology. FX Mix balances the two.",
        stages, connections
    };
    return schema;
}
}
