#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& stutterSchema()
{
    static constexpr const char* kLeftShiftIds[] = { "leftCents", "leftDelay", "leftFeedback" };
    static constexpr const char* kRightShiftIds[] = { "rightCents", "rightDelay", "rightFeedback" };
    static constexpr const char* kLeftStutterIds[] = { "length1", "count1", "triggerStutter1" };
    static constexpr const char* kRightStutterIds[] = { "length2", "count2", "triggerStutter2" };
    static constexpr const char* kSweepIds[] = { "up1Rate", "up1Max", "dn1Rate", "dn1Min", "rand1Max", "sweepTarget1", "triggerSweepUp1", "triggerSweepDown1", "triggerRandomPitch1", "up2Rate", "up2Max", "dn2Rate", "dn2Min", "rand2Max", "sweepTarget2", "triggerSweepUp2", "triggerSweepDown2", "triggerRandomPitch2" };
    static constexpr const char* kAutoIds[] = { "autoOn", "speed", "program" };
    static constexpr const char* kLeftOutIds[] = { "leftMix" };
    static constexpr const char* kRightOutIds[] = { "rightMix" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "leftShift", "L Pitch Shift", StageKind::kFeedback, "Coarse/Fine + Sweep 1/2 targeting Left", nullptr, kLeftShiftIds },
        { "rightShift", "R Pitch Shift", StageKind::kFeedback, "Coarse/Fine + Sweep 1/2 targeting Right", nullptr, kRightShiftIds },
        { "leftStutter", "L Stutter Control", StageKind::kProcessing, "triggered: loops captured window", nullptr, kLeftStutterIds },
        { "rightStutter", "R Stutter Control", StageKind::kProcessing, "triggered: loops captured window", nullptr, kRightStutterIds },
        { "sweeps", "Sweep Generators 1 & 2", StageKind::kProcessing,
          "Up/Down ramp or Random jump, each patchable to L/R/Both", nullptr, kSweepIds },
        { "auto", "Auto Sequencer", StageKind::kProcessing, "Speed + Program fire triggers automatically", nullptr, kAutoIds },
        { "leftOutput", "Left Output", StageKind::kOutput, "L Mix blends against dry", nullptr, kLeftOutIds },
        { "rightOutput", "Right Output", StageKind::kOutput, "R Mix blends against dry", nullptr, kRightOutIds },
    };
    static const Connection connections[] = {
        { "leftInput", "leftShift", nullptr },
        { "rightInput", "rightShift", nullptr },
        { "leftShift", "leftShift", "via L Delay", "L Feedback" },
        { "rightShift", "rightShift", "via R Delay", "R Feedback" },
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
