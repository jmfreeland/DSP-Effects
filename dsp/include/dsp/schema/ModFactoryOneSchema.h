#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& modFactoryOneSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "delays", "2 Sweepable Delays", StageKind::kFeedback, "BPM-synced, feedback, loop, mod" },
        { "filters", "2 State-Variable Filters", StageKind::kProcessing, "LP/BP/HP, mod-swept cutoff" },
        { "lfos", "2 Multi-Wave LFOs", StageKind::kProcessing, "13 waveforms, BPM-synced" },
        { "envelopes", "2 Envelope Detectors", StageKind::kProcessing, "envelope + ducker outputs" },
        { "ampmods", "2 Amplitude Modulators", StageKind::kProcessing, "VCAs driven by any source" },
        { "mixers", "4 Two-Input Mixers + Patch Matrix", StageKind::kFeedback,
          "28 destinations x 26 sources, one-sample latency" },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "mixers", "any patch destination" },
        { "rightInput", "mixers", "any patch destination" },
        { "mixers", "delays", "patchable" },
        { "mixers", "filters", "patchable" },
        { "mixers", "lfos", "patchable" },
        { "mixers", "envelopes", "patchable" },
        { "mixers", "ampmods", "patchable" },
        { "delays", "mixers", "patchable, one-sample latency" },
        { "filters", "mixers", "patchable, one-sample latency" },
        { "lfos", "mixers", "patchable, one-sample latency" },
        { "envelopes", "mixers", "patchable, one-sample latency" },
        { "ampmods", "mixers", "patchable, one-sample latency" },
        { "mixers", "leftOutput", "patchable" },
        { "mixers", "rightOutput", "patchable" },
    };
    static const AlgorithmSchema schema = {
        "mod factory|one",
        "A genuine modular patch-bay: a dozen-ish DSP modules (2 delays, 2 filters, 2 LFOs, 2 "
        "envelope/ducker detectors, 2 amplitude modulators, 4 mixers) wired by a settable 28x26 "
        "patch matrix - \"software patch cords\" that can build thousands of different effects.",
        stages, connections
    };
    return schema;
}
}
