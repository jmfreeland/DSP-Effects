#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& dualPltSchema()
{
    // Parameter IDs as authored in plugin/source/DualPltPluginProcessor.cpp,
    // validated against the live parameter list in debug builds.
    static constexpr const char* kInputIds[] = {
        "sends", "returns", "routing", "rvbInLevel", "fxInLevel",
    };
    static constexpr const char* kRvbIds[] = {
        "rvbMix", "decay", "lowRatio", "crossover", "damping", "diffusion", "size", "link",
        "attack", "rvbIn", "rvbOut", "preDelay",
        "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight", "spin",
        "ekoFeedbackLeft", "ekoFeedbackRight", "ekoDelayLeft", "ekoDelayRight",
    };
    static constexpr const char* kFxIds[] = {
        "fxMix", "splice",
        "voice0Delay", "voice0Cents", "voice0Feedback", "voice0CrossFeedback", "voice0Level", "voice0Pan",
        "voice1Delay", "voice1Cents", "voice1Feedback", "voice1CrossFeedback", "voice1Level", "voice1Pan",
    };
    static constexpr const char* kOutputIds[] = { "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Submixer Sends: Stereo/L=Rvb,R=FX/Mono/L=FX,R=Rvb",
          nullptr, kInputIds },
        { "rvb", "Plate", StageKind::kFeedback, "own In Lvl / Mix", nullptr, kRvbIds },
        { "fx", "Dual Shifter (2 voices)", StageKind::kFeedback,
          "own Delay/Pitch/Level/Pan; Fbk into own input, X-Fbk into the other voice's", nullptr,
          kFxIds },
        { "output", "Output", StageKind::kOutput,
          "Submixer Returns: Stereo/Rvb=L,FX=R/Mono/FX=L,Rvb=R; FX Width/Hi-Cut/Adjust/Mix", nullptr,
          kOutputIds },
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
        "Dual-Plt",
        "The second of the manual's five true Dual-FX Pitch algorithms (same shape as Dual-Chmb): a "
        "Submixer freely arranging a fixed Plate reverb against a 2-voice \"Dual Shifter\" FX block (each voice an independent "
        "Delay->Pitch->Level->Pan chain up to 1.25s, with Fbk/X-Fbk feedback between the two voices) via "
        "three controls - Sends (how the stereo input feeds each block), Returns (how each block's "
        "output feeds the main outputs), and Routing (Parallel / Rvb-into-FX series / FX-into-Rvb "
        "series, which takes precedence over Sends/Returns when active).",
        stages, connections
    };
    return schema;
}
}
