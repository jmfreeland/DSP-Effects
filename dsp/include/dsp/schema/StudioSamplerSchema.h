#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& studioSamplerSchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "sample1", "Sample 1", StageKind::kFeedback, "record/stop/play, Start-End range, Loop" },
        { "sample2", "Sample 2", StageKind::kFeedback, "record/stop/play, Start-End range, Loop" },
        { "shift1", "Attack/Release + Pitch/Time 1", StageKind::kProcessing, "generic or constant-length" },
        { "shift2", "Attack/Release + Pitch/Time 2", StageKind::kProcessing, "generic or constant-length" },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "sample1", nullptr },
        { "rightInput", "sample2", nullptr },
        { "sample1", "shift1", nullptr },
        { "sample2", "shift2", nullptr },
        { "shift1", "leftOutput", nullptr },
        { "shift2", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Studio Sampler",
        "Two fully independent per-channel samplers - record on command, then play back once or "
        "looped with independent Pitch and Time control (splicing in constant-length mode, simple "
        "varispeed in generic sampler mode) and their own Attack/Release envelope.",
        stages, connections
    };
    return schema;
}
}
