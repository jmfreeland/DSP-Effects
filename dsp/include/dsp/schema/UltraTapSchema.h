#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& ultraTapSchema()
{
    static constexpr const char* kInputIds[] = { "inLevelLeft", "inLevelRight", "stereoInput" };
    static constexpr const char* kDiffusorIds[] = { "diffusion", "allpass0Delay", "allpass1Delay", "allpass2Delay", "allpass3Delay" };
    static constexpr const char* kTapIds[] = { "length", "spacingShape", "weightsShape", "pansShape", "feedback", "fbTap", "tap0Delay", "tap0Level", "tap0Pan", "tap1Delay", "tap1Level", "tap1Pan", "tap2Delay", "tap2Level", "tap2Pan", "tap3Delay", "tap3Level", "tap3Pan", "tap4Delay", "tap4Level", "tap4Pan", "tap5Delay", "tap5Level", "tap5Pan", "tap6Delay", "tap6Level", "tap6Pan", "tap7Delay", "tap7Level", "tap7Pan", "tap8Delay", "tap8Level", "tap8Pan", "tap9Delay", "tap9Level", "tap9Pan", "tap10Delay", "tap10Level", "tap10Pan", "tap11Delay", "tap11Level", "tap11Pan" };
    static constexpr const char* kOutputIds[] = { "width", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Mono sum, or Left only - see Stereo/Mono", nullptr, kInputIds },
        { "diffusor", "Diffusor", StageKind::kProcessing, "4 cascaded Allpasses, own Delay + shared Diffusion", nullptr, kDiffusorIds },
        { "taps", "12-Tap Delay Line", StageKind::kProcessing,
          "Cumulative per-tap Delay/Level/Pan, one Fb Tap feeds back", nullptr, kTapIds },
        { "output", "Output", StageKind::kOutput, "Mix blends against dry", nullptr, kOutputIds },
    };
    static const Connection connections[] = {
        { "input", "diffusor", nullptr },
        { "diffusor", "taps", nullptr },
        { "taps", "output", nullptr },
        { "taps", "diffusor", "* Feedback - one selected tap" },
    };
    static const AlgorithmSchema schema = {
        "Ultra-Tap",
        "Two connected functions: a 4-stage Allpass diffusor generates a dense field of delays, which "
        "then feeds a 12-tap delay line (each tap's own Delay is the time since the previous tap, not "
        "from the input) with independent Level/Pan per tap, summed into a stereo mix.",
        stages, connections
    };
    return schema;
}
}
