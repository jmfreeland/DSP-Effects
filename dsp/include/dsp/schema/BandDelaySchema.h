#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& bandDelaySchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "sharedLine", "Multi-Tap Delay Line", StageKind::kFeedback,
          "one shared line, 8 independently-settable read taps" },
        { "filters", "8 Bandpass Filters", StageKind::kProcessing,
          "own Frequency/Q, Global Frequency/Q scale all 8" },
        { "outputs", "Output Levels & Pans", StageKind::kProcessing, "8 independent, then Global Pan" },
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
