#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& sweptCombsSchema()
{
    static constexpr const char* kInputIds[] = { "stereoInput" };
    static constexpr const char* kLineIds[] = { "masterDelay", "masterRate", "masterDepth", "masterFeedback", "line0Delay", "line0Rate", "line0Depth", "line0Feedback", "line0Level", "line0Pan", "line1Delay", "line1Rate", "line1Depth", "line1Feedback", "line1Level", "line1Pan", "line2Delay", "line2Rate", "line2Depth", "line2Feedback", "line2Level", "line2Pan", "line3Delay", "line3Rate", "line3Depth", "line3Feedback", "line3Level", "line3Pan", "line4Delay", "line4Rate", "line4Depth", "line4Feedback", "line4Level", "line4Pan", "line5Delay", "line5Rate", "line5Depth", "line5Feedback", "line5Level", "line5Pan" };
    static constexpr const char* kMixerIds[] = { "width", "repeat" };
    static constexpr const char* kOutputIds[] = { "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Mono sum, or Left only - see Stereo/Mono", nullptr, kInputIds },
        { "line1", "Line 1-6", StageKind::kFeedback,
          "Six independent swept combs: own Delay/Rate/Depth/Feedback/Pan/Level", nullptr, kLineIds },
        { "mixer", "Stereo Mixer", StageKind::kProcessing, "Sums all 6 panned lines", nullptr, kMixerIds },
        { "output", "Output", StageKind::kOutput, "Mix blends against dry", nullptr, kOutputIds },
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
