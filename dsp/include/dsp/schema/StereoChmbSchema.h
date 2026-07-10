#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& stereoChmbSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Submixer Sends: Stereo/L=Rvb,R=FX/Mono/L=FX,R=Rvb" },
        { "rvb", "Chamber", StageKind::kFeedback, "own In Lvl / Mix" },
        { "fx", "Stereo Shifter", StageKind::kFeedback,
          "one shared cents value drives independent L/R PitchShifters in lockstep; own per-channel feedback" },
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
        "Stereo-Chmb",
        "The fourth of the manual's five true Dual-FX Pitch algorithms: a Submixer freely arranging "
        "a fixed Chamber reverb against a \"Stereo Shifter\" FX block - unlike Dual-Chmb/Dual-Plt/"
        "Dual-Inv's two independent pitch-shift voices, this is \"a true stereo pitch shifter which "
        "maintains the stereo image of source material\": one shared cents value drives both channels "
        "sample-synchronously (reusing dsp::algorithms::StereoShift, already built for the Eventide "
        "H3000's own Algorithm 103), each channel's own feedback still returning only into itself.",
        stages, connections
    };
    return schema;
}
}
