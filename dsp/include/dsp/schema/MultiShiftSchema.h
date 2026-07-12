#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& multiShiftSchema()
{
    static constexpr const char* kLeftPitchIds[] = { "leftCents", "leftPitchDelay", "leftDirection", "leftXfadeSlow", "leftSplice", "leftFb1Amount", "leftFb1Source", "leftFb2Amount", "leftFb2Source", "lPitchLevel", "lPitchPan" };
    static constexpr const char* kRightPitchIds[] = { "rightCents", "rightPitchDelay", "rightDirection", "rightXfadeSlow", "rightSplice", "rightFb1Amount", "rightFb1Source", "rightFb2Amount", "rightFb2Source", "rPitchLevel", "rPitchPan" };
    static constexpr const char* kLeftDelayIds[] = { "leftDelay", "lDelayLevel", "lDelayPan" };
    static constexpr const char* kRightDelayIds[] = { "rightDelay", "rDelayLevel", "rDelayPan" };
    static constexpr const char* kOutputsIds[] = { "mix", "feedbackScale", "image" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "rightInput", "Right Input", StageKind::kInput, nullptr },
        { "leftPitch", "L Pitch", StageKind::kFeedback, "Delay->Shift, or Reverse->Shift; own Feedback 1/2", nullptr, kLeftPitchIds },
        { "rightPitch", "R Pitch", StageKind::kFeedback, "Delay->Shift, or Reverse->Shift; own Feedback 1/2", nullptr, kRightPitchIds },
        { "leftDelay", "L Delay", StageKind::kProcessing, "dry tap, no feedback", nullptr, kLeftDelayIds },
        { "rightDelay", "R Delay", StageKind::kProcessing, "dry tap, no feedback", nullptr, kRightDelayIds },
        { "outputs", "Output Levels & Pans", StageKind::kProcessing, "4 independent Level/Pan, then Image", nullptr, kOutputsIds },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry" },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "leftInput", "leftPitch", nullptr },
        { "rightInput", "rightPitch", nullptr },
        { "leftInput", "leftDelay", nullptr },
        { "rightInput", "rightDelay", nullptr },
        { "leftPitch", "leftPitch", "any 2 of 4 sources * Feedback" },
        { "rightPitch", "rightPitch", "any 2 of 4 sources * Feedback" },
        { "leftPitch", "outputs", nullptr },
        { "rightPitch", "outputs", nullptr },
        { "leftDelay", "outputs", nullptr },
        { "rightDelay", "outputs", nullptr },
        { "outputs", "leftOutput", nullptr },
        { "outputs", "rightOutput", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Multi-Shift",
        "Two independent pitch-shift channels (each a Delay->Shift chain, or a Reverse-splice->Shift "
        "chain in Reverse mode) plus two independent dry delay taps - four sources total, each with "
        "its own output Level/Pan. Each pitch shifter's own input can additionally be fed back from "
        "any two of the four sources, scaled by a master Feedback control.",
        stages, connections
    };
    return schema;
}
}
