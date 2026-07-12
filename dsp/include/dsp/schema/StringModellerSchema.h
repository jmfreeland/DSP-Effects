#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& stringModellerSchema()
{
    static constexpr const char* kStimFilterIds[] = { "freq", "qfac", "highAmt", "bandAmt", "lowAmt", "inAmt" };
    static constexpr const char* kVoiceIds[] = { "pitch", "decay", "gate", "bright", "trigger", "note1", "note2", "note3", "note4", "note5", "note6" };
    static constexpr const char* kChorusIds[] = { "chorus", "chorusSpeed", "chorusDepth" };
    static constexpr const char* kLeftOutIds[] = { "mix" };

    static const Stage stages[] = {
        { "leftInput", "Left Input", StageKind::kInput, nullptr },
        { "noise", "Noise Gen", StageKind::kInput, "excites the stimulation filter" },
        { "stimFilter", "Stimulation Filter", StageKind::kProcessing,
          "simultaneous Low/Band/High, mixed by Low/Band/High Amt", nullptr, kStimFilterIds },
        { "voices", "6 String Voices", StageKind::kFeedback,
          "Karplus-Strong: Delay + damping Filter + feedback, own Note tuning", nullptr, kVoiceIds },
        { "chorus", "Chorus", StageKind::kProcessing, "modulated delay, +/- combine to stereo", nullptr, kChorusIds },
        { "leftOutput", "Left Output", StageKind::kOutput, "Mix blends against dry", nullptr, kLeftOutIds },
        { "rightOutput", "Right Output", StageKind::kOutput, "Mix blends against dry" },
    };
    static const Connection connections[] = {
        { "noise", "stimFilter", nullptr },
        { "stimFilter", "voices", "Low/Band/High Amt" },
        { "leftInput", "voices", "In Amt (passive resonator)" },
        { "voices", "chorus", "6-voice sum" },
        { "chorus", "leftOutput", "+delayed" },
        { "chorus", "rightOutput", "-delayed" },
    };
    static const AlgorithmSchema schema = {
        "String Modeller",
        "Six Karplus-Strong string resonators, continuously excited by filtered noise and/or the "
        "live input (a manual \"pluck\" trigger substitutes for the original's MIDI note-on), "
        "feeding a modulated-delay Chorus that widens the mono resonator sum to stereo.",
        stages, connections
    };
    return schema;
}
}
