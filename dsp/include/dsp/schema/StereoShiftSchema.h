#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& stereoShiftSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "leftDelay", "L Delay", StageKind::kProcessing, "Shared Delay value" },
        { "rightDelay", "R Delay", StageKind::kProcessing, "Shared Delay value" },
        { "leftShift", "Left Shift", StageKind::kProcessing, "Shared cents shift, no pitch tracking" },
        { "rightShift", "Right Shift", StageKind::kProcessing, "Shared cents shift, no pitch tracking" },
        { "leftOutput", "Left Output", StageKind::kOutput, "Shared Mix blends Left Shift against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Shared Mix blends Right Shift against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "leftDelay", nullptr },
        { "rightInput", "rightDelay", nullptr },
        { "leftDelay", "leftShift", nullptr },
        { "rightDelay", "rightShift", nullptr },
        { "leftShift", "leftOutput", nullptr },
        { "rightShift", "rightOutput", nullptr },
        { "leftShift", "leftInput", "shared, own channel only", "Feedback" },
        { "rightShift", "rightInput", "shared, own channel only", "Feedback" },
    };
    static const AlgorithmSchema schema = {
        "Stereo Shift",
        "A true stereo pitch shifter: Left and Right channels never mix, but one shared Coarse/Fine, "
        "Delay, Feedback, and Mix value drives both - a genuine stereo pair processed identically, "
        "unlike Dual Shift's two independently-set channels.",
        stages, connections
    };
    return schema;
}
}
