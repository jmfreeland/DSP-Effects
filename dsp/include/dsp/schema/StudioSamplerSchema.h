#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& studioSamplerSchema()
{
    static constexpr const char* kSample1Ids[] = { "start1", "end1", "loop1", "attack1", "release1", "threshold1", "triggerMode1", "record1", "stop1", "play1" };
    static constexpr const char* kSample2Ids[] = { "start2", "end2", "loop2", "attack2", "release2", "threshold2", "triggerMode2", "record2", "stop2", "play2" };
    static constexpr const char* kShift1Ids[] = { "pitch1", "time1", "shiftMode1" };
    static constexpr const char* kShift2Ids[] = { "pitch2", "time2", "shiftMode2" };
    static constexpr const char* kLeftOutIds[] = { "mix" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "sample1", "Sample 1", StageKind::kFeedback, "record/stop/play, Start-End range, Loop", nullptr, kSample1Ids },
        { "sample2", "Sample 2", StageKind::kFeedback, "record/stop/play, Start-End range, Loop", nullptr, kSample2Ids },
        { "shift1", "Attack/Release + Pitch/Time 1", StageKind::kProcessing, "generic or constant-length", nullptr, kShift1Ids },
        { "shift2", "Attack/Release + Pitch/Time 2", StageKind::kProcessing, "generic or constant-length", nullptr, kShift2Ids },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry", nullptr, kLeftOutIds },
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
