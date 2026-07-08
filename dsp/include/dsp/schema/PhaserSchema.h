#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& phaserSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, "optional envelope sidechain only" },
        { "sweep", "Sweep Source", StageKind::kProcessing, "LFO, Envelope Follower, or ADSR" },
        { "stages", "12 Allpass Filters", StageKind::kFeedback, "in series, all swept together" },
        { "leftOutput", "Left Output", StageKind::kOutput, "dry*(1-Mix) + wet*Mix" },
        { "rightOutput", "Right Output", StageKind::kOutput, "wet*Mix only, no dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "stages", "+Feedback*previous output" },
        { "rightInput", "sweep", "if Envelope Channel = sidechain" },
        { "leftInput", "sweep", "if Envelope Channel = signal" },
        { "sweep", "stages", "sets allpass cutoff" },
        { "stages", "leftOutput", nullptr },
        { "stages", "rightOutput", nullptr },
        { "leftInput", "leftOutput", "dry*(1-Mix)" },
    };
    static const AlgorithmSchema schema = {
        "Phaser",
        "Twelve allpass filters in series, all swept to the same corner frequency by an LFO, "
        "envelope follower, or ADSR, then mixed back with the dry signal to produce moving "
        "notches - asymmetrically: the left output blends dry and wet, the right is wet only.",
        stages, connections
    };
    return schema;
}
}
