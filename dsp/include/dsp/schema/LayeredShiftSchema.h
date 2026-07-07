#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& layeredShiftSchema()
{
    static const Stage stages[] = {
        { "input", "Left Input", StageKind::kInput, "Right In not used - see doc comment" },
        { "leftDelay", "L Delay", StageKind::kProcessing, "0-1s ahead of Left Shift" },
        { "rightDelay", "R Delay", StageKind::kProcessing, "0-1s ahead of Right Shift" },
        { "leftShift", "Left Shift", StageKind::kProcessing, "Fixed cents shift, no pitch tracking" },
        { "rightShift", "Right Shift", StageKind::kProcessing, "Fixed cents shift, no pitch tracking" },
        { "output", "Output", StageKind::kOutput, "L/R Mix blends each Voice against dry" },
    };
    static const Connection connections[] = {
        { "input", "leftDelay", nullptr },
        { "input", "rightDelay", nullptr },
        { "leftDelay", "leftShift", nullptr },
        { "rightDelay", "rightShift", nullptr },
        { "leftShift", "output", nullptr },
        { "rightShift", "output", nullptr },
        { "leftShift", "input", "* L Feedback - cascades another lap" },
        { "rightShift", "input", "* R Feedback - cascades another lap" },
    };
    static const AlgorithmSchema schema = {
        "Layered Shift",
        "Two independent, fixed-interval pitch shifters both driven from the left input alone "
        "(no pitch tracking, unlike Diatonic Shift) - a direct Coarse/Fine cents shift per Voice, "
        "each with its own Delay and feedback back into the shared input, per the manual's own "
        "'instant 3 part harmony' description.",
        stages, connections
    };
    return schema;
}
}
