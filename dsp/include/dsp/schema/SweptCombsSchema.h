#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& sweptCombsSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Mono sum, or Left only - see Stereo/Mono" },
        { "line1", "Line 1-6", StageKind::kFeedback,
          "Six independent swept combs: own Delay/Rate/Depth/Feedback/Pan/Level" },
        { "mixer", "Stereo Mixer", StageKind::kProcessing, "Sums all 6 panned lines" },
        { "output", "Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "input", "line1", nullptr },
        { "line1", "mixer", nullptr },
        { "mixer", "output", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Swept Combs",
        "Six independent feedback delay lines, each with its own randomly-wandering (not sine/"
        "triangle) delay-length sweep, panned into a stereo mixer. Five Master controls (Delay/Rate/"
        "Depth/Feedback/Width) proportionally scale all six lines' own values without altering them, "
        "so returning a Master to 100% restores exactly what was set per line.",
        stages, connections
    };
    return schema;
}
}
