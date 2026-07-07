#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& dualShiftSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "leftDelay", "L Delay", StageKind::kProcessing, "0-500ms ahead of Left Shift" },
        { "rightDelay", "R Delay", StageKind::kProcessing, "0-500ms ahead of Right Shift" },
        { "leftShift", "Left Shift", StageKind::kProcessing, "Fixed cents shift, no pitch tracking" },
        { "rightShift", "Right Shift", StageKind::kProcessing, "Fixed cents shift, no pitch tracking" },
        { "leftOutput", "Left Output", StageKind::kOutput, "L Mix blends Left Shift against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "R Mix blends Right Shift against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "leftDelay", nullptr },
        { "rightInput", "rightDelay", nullptr },
        { "leftDelay", "leftShift", nullptr },
        { "rightDelay", "rightShift", nullptr },
        { "leftShift", "leftOutput", nullptr },
        { "rightShift", "rightOutput", nullptr },
        { "leftShift", "leftInput", "* L Feedback - own channel only" },
        { "rightShift", "rightInput", "* R Feedback - own channel only" },
    };
    static const AlgorithmSchema schema = {
        "Dual Shift",
        "Two completely separate pitch shifters - Left channel in/out and Right channel in/out never "
        "interact, each with its own Delay/cents/Feedback/Mix. Unlike Layered Shift, there is no shared "
        "input or shared feedback point.",
        stages, connections
    };
    return schema;
}
}
