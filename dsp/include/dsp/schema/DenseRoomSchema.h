#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& denseRoomSchema()
{
    static constexpr const char* kPredelayIds[] = { "predelay" };
    static constexpr const char* kDiffusionIds[] = { "diffusion", "allpassDelay1", "allpassDelay2", "allpassDelay3" };
    static constexpr const char* kTankIds[] = { "revTime", "highCut", "size", "lineDelay1", "lineDelay2", "lineDelay3", "lineDelay4", "lineDelay5", "lineDelay6", "linePan1", "linePan2", "linePan3", "linePan4", "linePan5", "linePan6", "lineLevel1", "lineLevel2", "lineLevel3", "lineLevel4", "lineLevel5", "lineLevel6" };
    static constexpr const char* kEarlyIds[] = { "position", "pan", "earlyMix" };
    static constexpr const char* kLeftOutIds[] = { "mix" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "predelay", "Predelay", StageKind::kProcessing, "0-500ms", nullptr, kPredelayIds },
        { "diffusion", "Diffusion", StageKind::kProcessing, "3-stage allpass, own per-stage Delay", nullptr, kDiffusionIds },
        { "tank", "Reverberator", StageKind::kFeedback,
          "6 lines: own Delay/Pan/Level, High Cut, Rev Time, Size", nullptr, kTankIds },
        { "earlyMixPan", "Early Mix / Pan", StageKind::kProcessing, "Position blends Early vs Tank", nullptr, kEarlyIds },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry", nullptr, kLeftOutIds },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "predelay", nullptr },
        { "predelay", "diffusion", nullptr },
        { "diffusion", "tank", nullptr },
        { "tank", "tank", "per-line", "Rev Time" },
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
