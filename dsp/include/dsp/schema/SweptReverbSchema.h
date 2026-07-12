#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& sweptReverbSchema()
{
    static constexpr const char* kLineIds[] = { "masterDelay", "masterRate", "masterDepth", "line0Delay", "line0Rate", "line0Depth", "line1Delay", "line1Rate", "line1Depth", "line2Delay", "line2Rate", "line2Depth", "line3Delay", "line3Rate", "line3Depth", "line4Delay", "line4Rate", "line4Depth", "line5Delay", "line5Rate", "line5Depth" };
    static constexpr const char* kNetworkIds[] = { "feedback", "repeat" };
    static constexpr const char* kOutputIds[] = { "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Summed to mono" },
        { "lines", "Delay 1-6", StageKind::kFeedback,
          "Six independently-swept lines, own Delay/Rate/Depth", nullptr, kLineIds },
        { "network", "Reverb Network", StageKind::kFeedback,
          "Householder mix (same technique as the PCM81 tank) + shared Feedback", nullptr, kNetworkIds },
        { "output", "Output", StageKind::kOutput, "Mix blends against dry", nullptr, kOutputIds },
    };
    static const Connection connections[] = {
        { "input", "lines", nullptr },
        { "lines", "network", "tapped" },
        { "network", "lines", "* Feedback - closes the loop" },
        { "lines", "output", "tapped" },
    };
    static const AlgorithmSchema schema = {
        "Swept Reverb",
        "Shares Swept Combs' six-independently-swept-delay-line shape, but the lines feed a "
        "Householder-mixed feedback network (the same diffusion technique this archive's Lexicon "
        "PCM81 reverb cores use for their own tank) instead of a stereo mixer, turning six discrete "
        "sweeping echoes into a continuous, modulated reverb tail.",
        stages, connections
    };
    return schema;
}
}
