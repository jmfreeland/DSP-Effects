#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& res2PlateSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan, Voice Diffusion" },
        { "tracker", "Pitch Detector (mono sum)", StageKind::kFeedback,
          "autocorrelation-based tracker, holds last pitch through silence" },
        { "resonators", "6 Resonator Voices (3 left / 3 right)", StageKind::kFeedback,
          "own HarmonicInterval/Level/Pan/Duration/HiCut per voice, retuned each hop to a diatonic "
          "degree relative to the tracked note" },
        { "reverb", "Plate (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank" },
        { "output", "Output", StageKind::kOutput, "FX Mix blends pre-reverb vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "tracker", "mono sum of L+R" },
        { "input", "resonators", nullptr },
        { "tracker", "resonators", "detected fundamental -> scale-degree math -> per-voice target Hz" },
        { "resonators", "resonators", "continuously excited by the live input, not a plucked trigger" },
        { "resonators", "reverb", "series - the six resonators' output becomes the reverb's input" },
        { "resonators", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "Res2>Plate",
        "The fifth and last of the manual's 6-Voice algorithms, and Res1>Plate's diatonic sibling: "
        "six Karplus-Strong-style resonator voices (three excited by the left input, three by the "
        "right), each retuned every pitch-detection hop to a chosen HarmonicInterval relative to the "
        "live-tracked input note - not a fixed chromatic voicing - whose combined output runs in "
        "series into a fixed Plate reverb. The manual assigns resonator pitch diatonically via a "
        "MIDI-tracked note; since this project has no MIDI input pathway, an audio-domain "
        "PitchDetector (reused from the Eventide H3000's Diatonic Shift) takes its place.",
        stages, connections
    };
    return schema;
}
}
