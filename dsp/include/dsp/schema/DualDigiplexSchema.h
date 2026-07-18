#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& dualDigiplexSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, "Unused in mono mode - see Stereo/Mono" },
        { "leftDelay", "L Delay", StageKind::kFeedback, "0-0.7s, own Feedback/Glide" },
        { "rightDelay", "R Delay", StageKind::kFeedback, "0-0.7s, own Feedback/Glide" },
        { "leftOutput", "Left Output", StageKind::kOutput, "L Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "R Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "leftDelay", nullptr },
        { "rightInput", "rightDelay", "stereo mode only" },
        { "leftInput", "rightDelay", "mono mode only" },
        { "leftDelay", "leftOutput", nullptr },
        { "rightDelay", "rightOutput", nullptr },
        { "leftDelay", "leftDelay", nullptr, "L Feedback" },
        { "rightDelay", "rightDelay", nullptr, "R Feedback" },
    };
    static const AlgorithmSchema schema = {
        "Dual Digiplex",
        "Two independent Long-Digiplex-style delay lines, each with its own Delay/Feedback/Mix. In "
        "Stereo mode each channel reads its own input; in Mono mode both delay lines read the left "
        "input alone.",
        stages, connections
    };
    return schema;
}
}
