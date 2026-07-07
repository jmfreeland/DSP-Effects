#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& patchFactorySchema()
{
    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "noiseGen", "Noise Gen", StageKind::kInput, "white noise" },
        { "scale2", "Scale 2", StageKind::kProcessing, "-100..100%, default patch source" },
        { "scale1", "Scale 1", StageKind::kProcessing, "-100..100%, default patch source" },
        { "sum2", "Sum 2", StageKind::kProcessing, "Scaler2 + b (Null)" },
        { "sum1", "Sum 1", StageKind::kProcessing, "Scaler1 + b (Null), unused by default output" },
        { "delay1", "Delay 1", StageKind::kProcessing, "0-0.5s" },
        { "delay2", "Delay 2", StageKind::kProcessing, "0-0.5s, unused by default output" },
        { "filter1", "Filter 1", StageKind::kProcessing, "SVF: LP/BP/HP taps all patchable, Cutoff/Q" },
        { "shift", "Pitch Shift", StageKind::kProcessing, "own P Delay grain buffer" },
        { "filter2", "Filter 2", StageKind::kProcessing, "SVF: LP/BP/HP taps all patchable, Cutoff/Q" },
        { "leftOutput", "Left Output", StageKind::kOutput, "L Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "R Mix blends against dry left" },
    };
    static const Connection connections[] = {
        { "noiseGen", "scale1", nullptr },
        { "leftInput", "scale2", nullptr },
        { "scale1", "sum1", nullptr },
        { "scale2", "sum2", nullptr },
        { "sum2", "delay1", "default patch - any source may be re-patched" },
        { "leftInput", "delay2", "default patch, unused downstream" },
        { "delay1", "filter1", "default patch" },
        { "leftInput", "shift", "default patch" },
        { "shift", "filter2", "default patch" },
        { "filter1", "leftOutput", "default patch: Lowpass1 tap" },
        { "filter2", "rightOutput", "default patch: Lowpass2 tap" },
    };
    static const AlgorithmSchema schema = {
        "Patch Factory",
        "A modular patch-bay - noise generator, two switchable-tap filters, two delay lines, one "
        "pitch shifter, two scalers, two summing junctions - wired by a settable 13-destination x "
        "16-source patch matrix. This diagram shows the factory default patch; every connection "
        "shown (and many more) is re-patchable via setPatch().",
        stages, connections
    };
    return schema;
}
}
