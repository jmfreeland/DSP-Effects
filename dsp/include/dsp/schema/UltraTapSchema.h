#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& ultraTapSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Mono sum, or Left only - see Stereo/Mono" },
        { "diffusor", "Diffusor", StageKind::kProcessing, "4 cascaded Allpasses, own Delay + shared Diffusion" },
        { "taps", "12-Tap Delay Line", StageKind::kProcessing,
          "Cumulative per-tap Delay/Level/Pan, one Fb Tap feeds back" },
        { "output", "Output", StageKind::kOutput, "Mix blends against dry" },
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
