#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& quadHallSchema()
{
    // Parameter IDs as authored in plugin/source/QuadHallPluginProcessor.cpp,
    // validated against the live parameter list in debug builds. Splice
    // (the shifters' shared grain length) is deliberately left unlisted:
    // it belongs to all four voices at once, and this schema splits the
    // voices across two stages - it lands in "More Parameters" instead of
    // being arbitrarily assigned to one bus.
    static constexpr const char* kInputIds[] = {
        "inLevelLeft", "inLevelRight", "inPanLeft", "inPanRight",
    };
    static constexpr const char* kLeftVoiceIds[] = {
        "voice0Delay", "voice0Cents", "voice0Feedback", "voice0CrossFeedback", "voice0Level", "voice0Pan",
        "voice1Delay", "voice1Cents", "voice1Feedback", "voice1CrossFeedback", "voice1Level", "voice1Pan",
    };
    static constexpr const char* kRightVoiceIds[] = {
        "voice2Delay", "voice2Cents", "voice2Feedback", "voice2CrossFeedback", "voice2Level", "voice2Pan",
        "voice3Delay", "voice3Cents", "voice3Feedback", "voice3CrossFeedback", "voice3Level", "voice3Pan",
    };
    static constexpr const char* kReverbIds[] = {
        "decay", "lowRatio", "crossover", "damping", "diffusion", "size", "link",
        "definition", "depth", "rvbIn", "rvbOut", "preDelay",
        "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight", "spin", "chorus", "freeze",
    };
    static constexpr const char* kOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan", nullptr, kInputIds },
        { "leftVoices", "Voices 1-2 (Left)", StageKind::kFeedback,
          "own Delay/Pitch/Level/Pan; Fbk into own bus, X-Fbk into the right bus", nullptr,
          kLeftVoiceIds },
        { "rightVoices", "Voices 3-4 (Right)", StageKind::kFeedback,
          "own Delay/Pitch/Level/Pan; Fbk into own bus, X-Fbk into the left bus", nullptr,
          kRightVoiceIds },
        { "reverb", "Concert Hall (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank",
          nullptr, kReverbIds },
        { "output", "Output", StageKind::kOutput, "FX Mix blends dry-shifted vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix",
          nullptr, kOutputIds },
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
