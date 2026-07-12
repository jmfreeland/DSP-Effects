#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& patchFactorySchema()
{
    static constexpr const char* kFilter1Ids[] = { "cutoff1", "q1", "filter1In" };
    static constexpr const char* kFilter2Ids[] = { "cutoff2", "q2", "filter2In" };
    static constexpr const char* kDelay1Ids[] = { "delay1", "delay1In" };
    static constexpr const char* kDelay2Ids[] = { "delay2", "delay2In" };
    static constexpr const char* kScale1Ids[] = { "scale1", "scale1In" };
    static constexpr const char* kScale2Ids[] = { "scale2", "scale2In" };
    static constexpr const char* kSum1Ids[] = { "sum1aIn", "sum1bIn" };
    static constexpr const char* kSum2Ids[] = { "sum2aIn", "sum2bIn" };
    static constexpr const char* kShiftIds[] = { "shiftCents", "pitchDelay", "shiftIn" };
    static constexpr const char* kLeftOutIds[] = { "lOutput", "leftMix" };
    static constexpr const char* kRightOutIds[] = { "rOutput", "rightMix" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "noiseGen", "Noise Gen", StageKind::kInput, "white noise" },
        { "scale2", "Scale 2", StageKind::kProcessing, "-100..100%, default patch source", nullptr, kScale2Ids },
        { "scale1", "Scale 1", StageKind::kProcessing, "-100..100%, default patch source", nullptr, kScale1Ids },
        { "sum2", "Sum 2", StageKind::kProcessing, "Scaler2 + b (Null)", nullptr, kSum2Ids },
        { "sum1", "Sum 1", StageKind::kProcessing, "Scaler1 + b (Null), unused by default output", nullptr, kSum1Ids },
        { "delay1", "Delay 1", StageKind::kProcessing, "0-0.5s", nullptr, kDelay1Ids },
        { "delay2", "Delay 2", StageKind::kProcessing, "0-0.5s, unused by default output", nullptr, kDelay2Ids },
        { "filter1", "Filter 1", StageKind::kProcessing, "SVF: LP/BP/HP taps all patchable, Cutoff/Q", nullptr, kFilter1Ids },
        { "shift", "Pitch Shift", StageKind::kProcessing, "own P Delay grain buffer", nullptr, kShiftIds },
        { "filter2", "Filter 2", StageKind::kProcessing, "SVF: LP/BP/HP taps all patchable, Cutoff/Q", nullptr, kFilter2Ids },
        { "leftOutput", "Left Output", StageKind::kOutput, "L Mix blends against dry", nullptr, kLeftOutIds },
        { "rightOutput", "Right Output", StageKind::kOutput, "R Mix blends against dry left", nullptr, kRightOutIds },
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
