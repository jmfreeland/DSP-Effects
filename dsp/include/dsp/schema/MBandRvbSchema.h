#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& mBandRvbSchema()
{
    // Parameter IDs as authored in plugin/source/MBandRvbPluginProcessor.cpp,
    // validated against the live parameter list in debug builds. Diffusion
    // is the manual's own shared control (reverb + the voices' feedback
    // loops both), so it lives on the diffusion stage.
    static constexpr const char* kInputIds[] = {
        "inLevelLeft", "inLevelRight", "inPanLeft", "inPanRight",
    };
    static constexpr const char* kDiffusionIds[] = { "diffusion" };
    static constexpr const char* kVoiceBankIds[] = {
        "voice0Delay", "voice0Level", "voice0Pan", "voice0Feedback", "voice0HiCut", "voice0LoCut",
        "voice1Delay", "voice1Level", "voice1Pan", "voice1Feedback", "voice1HiCut", "voice1LoCut",
        "voice2Delay", "voice2Level", "voice2Pan", "voice2Feedback", "voice2HiCut", "voice2LoCut",
        "voice3Delay", "voice3Level", "voice3Pan", "voice3Feedback", "voice3HiCut", "voice3LoCut",
        "voice4Delay", "voice4Level", "voice4Pan", "voice4Feedback", "voice4HiCut", "voice4LoCut",
        "voice5Delay", "voice5Level", "voice5Pan", "voice5Feedback", "voice5HiCut", "voice5LoCut",
    };
    static constexpr const char* kReverbIds[] = {
        "decay", "lowRatio", "crossover", "damping", "size", "link", "shape", "spread",
        "rvbOut", "preDelay",
        "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight",
        "ekoDelayLeft", "ekoDelayRight", "ekoFeedbackLeft", "ekoFeedbackRight", "spin", "freeze",
    };
    static constexpr const char* kOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan", nullptr, kInputIds },
        { "diffusion", "Diffusion (shared w/ reverb, inside feedback loop)", StageKind::kFeedback,
          "2x Allpass per channel", nullptr, kDiffusionIds },
        { "voiceBank", "6-Voice Multiband (3 left / 3 right)", StageKind::kFeedback,
          "own Delay/Level/Pan/Feedback/HiCut/LoCut per voice", nullptr, kVoiceBankIds },
        { "reverb", "Chamber (fixed, in parallel)", StageKind::kFeedback, "8-line Householder FDN tank + pre-echo",
          nullptr, kReverbIds },
        { "output", "Output", StageKind::kOutput, "FX Mix blends multiband vs. reverb; FX Width/Hi-Cut/Adjust/Mix",
          nullptr, kOutputIds },
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
