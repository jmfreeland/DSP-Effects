#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& reverseShiftSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Summed to mono" },
        { "leftReverse", "L Reverse", StageKind::kProcessing, "Tape-reverse splice, own Length" },
        { "rightReverse", "R Reverse", StageKind::kProcessing, "Tape-reverse splice, own Length" },
        { "leftShift", "Left Shift", StageKind::kProcessing, "Cents shift on top of the reversal" },
        { "rightShift", "Right Shift", StageKind::kProcessing, "Cents shift on top of the reversal" },
        { "output", "Output", StageKind::kOutput, "L/R Mix blends each Voice against dry" },
    };
    static const Connection connections[] = {
        { "input", "leftReverse", nullptr },
        { "input", "rightReverse", nullptr },
        { "leftReverse", "leftShift", nullptr },
        { "rightReverse", "rightShift", nullptr },
        { "leftShift", "output", nullptr },
        { "rightShift", "output", nullptr },
        { "leftShift", "input", "cascades another lap", "L Feedback" },
        { "rightShift", "input", "cascades another lap", "R Feedback" },
    };
    static const AlgorithmSchema schema = {
        "Reverse Shift",
        "One-channel-in, two-channels-out: each Voice records a settable-length splice and plays it "
        "back time-reversed (a 'tape reverse' generator, not a continuous pitch sweep), then layers an "
        "independent cents-based pitch shift on top - genuinely different from every other H3000 "
        "pitch-shift algorithm in this archive, which all shift smoothly rather than reversing.",
        stages, connections
    };
    return schema;
}
}
