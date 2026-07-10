#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& vsoChmbSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Submixer Sends: Stereo/L=Rvb,R=FX/Mono/L=FX,R=Rvb" },
        { "rvb", "Chamber", StageKind::kFeedback, "own In Lvl / Mix" },
        { "fx", "Stereo Shifter", StageKind::kFeedback,
          "Varispeed (%) -> compensating cents; one shared value drives independent L/R PitchShifters in lockstep" },
        { "output", "Output", StageKind::kOutput,
          "Submixer Returns: Stereo/Rvb=L,FX=R/Mono/FX=L,Rvb=R; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "rvb", "Sends" },
        { "input", "fx", "Sends" },
        { "fx", "fx", "own-channel feedback (L->L, R->R)" },
        { "rvb", "output", "Returns (Parallel routing)" },
        { "fx", "output", "Returns (Parallel routing)" },
        { "rvb", "fx", "Routing = Rvb into FX (series)" },
        { "fx", "rvb", "Routing = FX into Rvb (series, reverse)" },
    };
    static const AlgorithmSchema schema = {
        "VSO-Chmb",
        "The fifth and last of the manual's true Dual-FX Pitch algorithms - identical to Stereo-Chmb "
        "(\"Like the Stereo-Chmb algorithm, VSO-Chmb is combined with a stereo chamber reverb\") plus "
        "one Varispeed parameter (+55.00% to -35.00%): \"a utility program designed to provide pitch "
        "correction of varispeed material... match the value of the Varispeed parameter to the "
        "varispeed setting of the playback source.\" A closed-form percentage-to-cents mapping "
        "(speedMultiplier = 1/(1-varispeed/100), shiftCents = 1200*log2(1-varispeed/100)) drives the "
        "same Stereo Shifter FX block unchanged - no new DSP mechanism, just a different way to set "
        "one existing parameter.",
        stages, connections
    };
    return schema;
}
}
