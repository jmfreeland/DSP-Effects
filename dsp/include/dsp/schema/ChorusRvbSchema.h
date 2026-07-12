#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& chorusRvbSchema()
{
    // Parameter IDs as authored in plugin/source/ChorusRvbPluginProcessor.cpp,
    // validated against the live parameter list in debug builds. The shared
    // Diffusion (the manual's own callout: one control serving both the
    // chorus voices and the reverb) lives on the diffusion stage.
    static constexpr const char* kInputIds[] = {
        "inLevelLeft", "inLevelRight", "inPanLeft", "inPanRight",
    };
    static constexpr const char* kDiffusionIds[] = { "diffusion" };
    static constexpr const char* kHighCutIds[] = { "chorusHighCut" };
    static constexpr const char* kVoiceBankIds[] = {
        "chorusMasterDepth", "chorusMasterRate",
        "voice0Delay", "voice0Level", "voice0Pan", "voice0Feedback", "voice0Depth", "voice0Rate",
        "voice1Delay", "voice1Level", "voice1Pan", "voice1Feedback", "voice1Depth", "voice1Rate",
        "voice2Delay", "voice2Level", "voice2Pan", "voice2Feedback", "voice2Depth", "voice2Rate",
        "voice3Delay", "voice3Level", "voice3Pan", "voice3Feedback", "voice3Depth", "voice3Rate",
        "voice4Delay", "voice4Level", "voice4Pan", "voice4Feedback", "voice4Depth", "voice4Rate",
        "voice5Delay", "voice5Level", "voice5Pan", "voice5Feedback", "voice5Depth", "voice5Rate",
    };
    static constexpr const char* kReverbIds[] = {
        "decay", "lowRatio", "crossover", "damping", "size", "link", "attack",
        "rvbOut", "preDelay",
        "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight",
        "ekoDelayLeft", "ekoDelayRight", "ekoFeedbackLeft", "ekoFeedbackRight", "spin", "freeze",
    };
    static constexpr const char* kOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan", nullptr, kInputIds },
        { "diffusion", "Diffusion (shared w/ reverb)", StageKind::kProcessing, "2x Allpass per channel",
          nullptr, kDiffusionIds },
        { "highCut", "Chorus High Cut", StageKind::kProcessing, "one-pole, Controls row", nullptr,
          kHighCutIds },
        { "voiceBank", "6-Voice Chorus (3 left / 3 right)", StageKind::kFeedback,
          "own Delay/Level/Pan/Feedback/Depth/Rate per voice", nullptr, kVoiceBankIds },
        { "reverb", "Plate (fixed, in parallel)", StageKind::kFeedback, "8-line Householder FDN tank + pre-echo",
          nullptr, kReverbIds },
        { "output", "Output", StageKind::kOutput, "FX Mix blends chorus vs. reverb; FX Width/Hi-Cut/Adjust/Mix",
          nullptr, kOutputIds },
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
