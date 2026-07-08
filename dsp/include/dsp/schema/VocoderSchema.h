#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& vocoderSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, "Synthesis (instrument)" },
        { "rightInput", "Right Input", StageKind::kInput, "Analysis (voice)" },
        { "analysis", "Analysis Filterbank", StageKind::kProcessing,
          "12 bands, per-band envelope: Formant Speed then Envelope Speed" },
        { "synthesisFilter", "Synthesis Filterbank", StageKind::kProcessing,
          "12 bands, scaled by Analysis envelopes, Formant Shift" },
        { "pseudoStereo", "Pseudo-Stereo", StageKind::kOutput, "Depth/Width" },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry synthesis" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry synthesis" },
    };
    static const Connection connections[] = {
        { "rightInput", "analysis", nullptr },
        { "leftInput", "synthesisFilter", nullptr },
        { "analysis", "synthesisFilter", "per-band gain" },
        { "synthesisFilter", "pseudoStereo", nullptr },
        { "pseudoStereo", "leftOutput", nullptr },
        { "pseudoStereo", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Vocoder",
        "A classic multi-band channel vocoder: a 12-band Analysis Filterbank tracks the voice "
        "input's per-band energy (Formant Speed then Envelope Speed smoothing), and an identically "
        "shaped Synthesis Filterbank processing the instrument input is scaled by those energies "
        "band-by-band before summing - the standard way to build a vocoder's \"impress one sound's "
        "spectral envelope onto another's\" behavior.",
        stages, connections
    };
    return schema;
}
}
