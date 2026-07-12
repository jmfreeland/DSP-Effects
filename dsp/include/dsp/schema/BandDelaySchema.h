#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& bandDelaySchema()
{
    static constexpr const char* kSharedLineIds[] = { "globalDelay", "delay1", "delay2", "delay3", "delay4", "delay5", "delay6", "delay7", "delay8", "feedbackDelay", "feedback" };
    static constexpr const char* kFilterIds[] = { "globalFrequency", "globalQ", "baseHz1", "baseHz2", "baseHz3", "baseHz4", "baseHz5", "baseHz6", "baseHz7", "baseHz8", "cents1", "cents2", "cents3", "cents4", "cents5", "cents6", "cents7", "cents8", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "q8" };
    static constexpr const char* kOutputsIds[] = { "globalPan", "level1", "level2", "level3", "level4", "level5", "level6", "level7", "level8", "pan1", "pan2", "pan3", "pan4", "pan5", "pan6", "pan7", "pan8", "mix" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "sharedLine", "Multi-Tap Delay Line", StageKind::kFeedback,
          "one shared line, 8 independently-settable read taps", nullptr, kSharedLineIds },
        { "filters", "8 Bandpass Filters", StageKind::kProcessing,
          "own Frequency/Q, Global Frequency/Q scale all 8", nullptr, kFilterIds },
        { "outputs", "Output Levels & Pans", StageKind::kProcessing, "8 independent, then Global Pan", nullptr, kOutputsIds },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "sharedLine", nullptr },
        { "rightInput", "sharedLine", nullptr },
        { "sharedLine", "filters", "8 taps, each own Delay * Global Delay" },
        { "filters", "outputs", nullptr },
        { "outputs", "sharedLine", "Feedback Delay * Feedback" },
        { "outputs", "leftOutput", nullptr },
        { "outputs", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Band Delay",
        "A multi-tap delay line - one shared buffer with 8 independently-settable read taps, each "
        "feeding its own bandpass filter, output Level, and Pan. A single recirculating feedback "
        "loop (its own Delay + Feedback amount) reads the final mixed output back into the shared "
        "line, forming a digital delay repeat loop.",
        stages, connections
    };
    return schema;
}
}
