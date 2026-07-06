#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& diatonicShiftSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "delay", "Delay", StageKind::kProcessing, "0-1.5s, spaces out successive repeats" },
        { "shifter", "Pitch Shifter", StageKind::kProcessing,
          "Delay-line/grain based, shift = Scale Degree quantized to Key+Scale" },
        { "output", "Output", StageKind::kOutput, "Mix blends shifted signal against dry" },
    };
    static const Connection connections[] = {
        { "input", "delay", nullptr },
        { "delay", "shifter", "tapped" },
        { "shifter", "output", nullptr },
        { "shifter", "delay", "* Regen - feeds back for another lap, one more diatonic step" },
    };
    static const AlgorithmSchema schema = {
        "Diatonic Shift",
        "Each lap through the Delay->Shifter->Regen loop adds another diatonic step, "
        "producing a cascading ascending/descending arpeggio rather than a static reverb tail.",
        stages, connections
    };
    return schema;
}
}
