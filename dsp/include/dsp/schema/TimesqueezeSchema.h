#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& timesqueezeSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "leftShift", "L Pitch Shift", StageKind::kProcessing, "Time% -> inverse speed ratio * Pitch trim" },
        { "rightShift", "R Pitch Shift", StageKind::kProcessing, "same shared ratio as Left" },
        { "leftOutput", "Left Output", StageKind::kOutput, "fully wet - no Mix control" },
        { "rightOutput", "Right Output", StageKind::kOutput, "fully wet - no Mix control" },
    };
    static const Connection connections[] = {
        { "leftInput", "leftShift", nullptr },
        { "rightInput", "rightShift", nullptr },
        { "leftShift", "leftOutput", nullptr },
        { "rightShift", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Timesqueeze",
        "A tape-speed pitch-correction utility: Time% (the length change a connected tape machine "
        "would be told to make) is converted to the speed ratio it implies, and both channels are "
        "shifted by its inverse - so a tape sped up 2x, which would otherwise raise pitch an octave, "
        "gets shifted back down an octave - times an independent Pitch trim ratio. No tape-machine "
        "control-voltage output exists in this software, so only the pitch-correction half is built.",
        stages, connections
    };
    return schema;
}
}
