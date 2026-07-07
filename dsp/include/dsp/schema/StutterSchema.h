#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& stutterSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "leftShift", "L Pitch Shift", StageKind::kFeedback, "Coarse/Fine + Sweep 1/2 targeting Left" },
        { "rightShift", "R Pitch Shift", StageKind::kFeedback, "Coarse/Fine + Sweep 1/2 targeting Right" },
        { "leftStutter", "L Stutter Control", StageKind::kProcessing, "triggered: loops captured window" },
        { "rightStutter", "R Stutter Control", StageKind::kProcessing, "triggered: loops captured window" },
        { "sweeps", "Sweep Generators 1 & 2", StageKind::kProcessing,
          "Up/Down ramp or Random jump, each patchable to L/R/Both" },
        { "auto", "Auto Sequencer", StageKind::kProcessing, "Speed + Program fire triggers automatically" },
        { "leftOutput", "Left Output", StageKind::kOutput, "L Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "R Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "leftShift", nullptr },
        { "rightInput", "rightShift", nullptr },
        { "leftShift", "leftShift", "* L Feedback (via L Delay)" },
        { "rightShift", "rightShift", "* R Feedback (via R Delay)" },
        { "leftShift", "leftStutter", nullptr },
        { "rightShift", "rightStutter", nullptr },
        { "sweeps", "leftShift", "Sweep Target 1/2" },
        { "sweeps", "rightShift", "Sweep Target 1/2" },
        { "auto", "sweeps", "Program: random sweep/pitch" },
        { "auto", "leftStutter", "Program: total random/just stutter" },
        { "auto", "rightStutter", "Program: total random/just stutter" },
        { "leftStutter", "leftOutput", nullptr },
        { "rightStutter", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Stutter",
        "A real-time \"st..st..stutter\" performance effect: per channel, a pitch shifter (base "
        "Coarse/Fine plus up to two sweep generators) feeds a delay-with-feedback stage into a "
        "Stutter Control that loops its most recently captured window when triggered. Two stutter "
        "presets and two independent sweep generators (up/down/random-pitch) can be fired manually "
        "or by an Auto sequencer.",
        stages, connections
    };
    return schema;
}
}
