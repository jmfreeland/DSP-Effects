#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& reverbFactorySchema()
{
    static constexpr const char* kInputIds[] = { "inLevelLeft", "inLevelRight" };
    static constexpr const char* kPredelayIds[] = { "predelay" };
    static constexpr const char* kGateIds[] = { "gateEnabled", "gateTime", "gateSpeed", "gateThreshold" };
    static constexpr const char* kLineIds[] = { "line0Delay", "line1Delay", "line2Delay", "line3Delay", "line4Delay", "line5Delay", "onDecay", "offDecay", "eqCrossover", "onEqGain", "offEqGain" };
    static constexpr const char* kOutputIds[] = { "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Summed to mono", nullptr, kInputIds },
        { "predelay", "Predelay", StageKind::kProcessing, "0-500ms ahead of the network", nullptr, kPredelayIds },
        { "gate", "Gate", StageKind::kProcessing,
          "Envelope follower + Threshold/Speed/Time - crossfades On/Off Decay+EQ", nullptr, kGateIds },
        { "lines", "Delay 1-6", StageKind::kFeedback, "Fixed per-line delays, own EQ crossover", nullptr, kLineIds },
        { "network", "Reverb Network", StageKind::kFeedback,
          "Householder mix + gate-crossfaded per-line decay/EQ" },
        { "output", "Output", StageKind::kOutput, "Mix blends against dry", nullptr, kOutputIds },
    };
    static const Connection connections[] = {
        { "input", "predelay", nullptr },
        { "input", "gate", "envelope" },
        { "predelay", "lines", nullptr },
        { "gate", "network", "crossfades Decay/EQ" },
        { "lines", "network", "tapped" },
        { "network", "lines", "closes the loop" },
        { "lines", "output", "tapped" },
    };
    static const AlgorithmSchema schema = {
        "Reverb Factory",
        "Shares the six-line Householder network with Swept Reverb, but the lines are fixed (not "
        "swept) and a dynamics Gate crossfades each line's decay time and tone between two "
        "independent settings - On (loud/above threshold) and Off (soft/below) - rather than a "
        "single fixed decay.",
        stages, connections
    };
    return schema;
}
}
