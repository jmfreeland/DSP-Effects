#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& denseRoomSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "predelay", "Predelay", StageKind::kProcessing, "0-500ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "3-stage allpass, own per-stage Delay" },
        { "tank", "Reverberator", StageKind::kFeedback,
          "6 lines: own Delay/Pan/Level, High Cut, Rev Time, Size" },
        { "earlyMixPan", "Early Mix / Pan", StageKind::kProcessing, "Position blends Early vs Tank" },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "predelay", nullptr },
        { "predelay", "diffusion", nullptr },
        { "diffusion", "tank", nullptr },
        { "tank", "tank", "* per-line Rev Time" },
        { "diffusion", "earlyMixPan", "Position: front" },
        { "tank", "earlyMixPan", "Position: rear" },
        { "earlyMixPan", "leftOutput", nullptr },
        { "earlyMixPan", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Dense Room",
        "A denser evolution of Reverb Factory's 6-line Householder tank: adds a Diffusion stage "
        "and explicit per-line Pan/Level, replaces the Gate with a single Rev Time, and replaces "
        "the parametric EQ with a simple High Cut. Position blends between the Diffusion stage's "
        "own output (front) and the tank's mixed output (rear); Early Mix blends each line's raw "
        "tap against its fully-mixed value.",
        stages, connections
    };
    return schema;
}
}
