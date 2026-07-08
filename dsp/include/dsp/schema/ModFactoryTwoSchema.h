#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& modFactoryTwoSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "delays", "2 Filtered Delays", StageKind::kFeedback, "BPM-synced, feedback, loop, highcut" },
        { "detuners", "2 Detuners", StageKind::kProcessing, "splice-based, small-range pitch shift" },
        { "lfo", "Multi-Wave LFO", StageKind::kProcessing, "13 waveforms, BPM-synced" },
        { "envelope", "Envelope Detector", StageKind::kProcessing, "envelope + ducker outputs" },
        { "ampmods", "2 Amplitude Modulators", StageKind::kProcessing, "VCAs driven by any source" },
        { "mixers", "4 Two-Input Mixers + Patch Matrix", StageKind::kFeedback,
          "28 destinations x 22 sources, one-sample latency" },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "mixers", "any patch destination" },
        { "rightInput", "mixers", "any patch destination" },
        { "mixers", "delays", "patchable" },
        { "mixers", "detuners", "patchable" },
        { "mixers", "lfo", "patchable" },
        { "mixers", "envelope", "patchable" },
        { "mixers", "ampmods", "patchable" },
        { "delays", "mixers", "patchable, one-sample latency" },
        { "detuners", "mixers", "patchable, one-sample latency" },
        { "lfo", "mixers", "patchable, one-sample latency" },
        { "envelope", "mixers", "patchable, one-sample latency" },
        { "ampmods", "mixers", "patchable, one-sample latency" },
        { "mixers", "leftOutput", "patchable" },
        { "mixers", "rightOutput", "patchable" },
    };
    static const AlgorithmSchema schema = {
        "mod factory|two",
        "A cousin to mod factory|one: 2 filtered delays, 2 detuning pitch shifters, one LFO, one "
        "envelope/ducker detector, 2 amplitude modulators, 4 mixers, wired by a settable 28x22 "
        "patch matrix.",
        stages, connections
    };
    return schema;
}
}
