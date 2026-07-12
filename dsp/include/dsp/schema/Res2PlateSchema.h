#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& res2PlateSchema()
{
    // Parameter IDs as authored in plugin/source/Res2PlatePluginProcessor.cpp,
    // validated against the live parameter list in debug builds.
    static constexpr const char* kInputIds[] = {
        "inLevelLeft", "inLevelRight", "inPanLeft", "inPanRight", "voiceDiffusion",
    };
    static constexpr const char* kTrackerIds[] = {
        "key", "scale", "tune", "lowNoteHz", "highNoteHz",
    };
    static constexpr const char* kResonatorIds[] = {
        "voice0Interval", "voice0Level", "voice0Pan", "voice0Duration", "voice0HiCut",
        "voice1Interval", "voice1Level", "voice1Pan", "voice1Duration", "voice1HiCut",
        "voice2Interval", "voice2Level", "voice2Pan", "voice2Duration", "voice2HiCut",
        "voice3Interval", "voice3Level", "voice3Pan", "voice3Duration", "voice3HiCut",
        "voice4Interval", "voice4Level", "voice4Pan", "voice4Duration", "voice4HiCut",
        "voice5Interval", "voice5Level", "voice5Pan", "voice5Duration", "voice5HiCut",
    };
    static constexpr const char* kReverbIds[] = {
        "decay", "lowRatio", "crossover", "damping", "diffusion", "size", "link", "attack",
        "rvbOut", "preDelay",
        "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight",
        "ekoDelayLeft", "ekoDelayRight", "ekoFeedbackLeft", "ekoFeedbackRight", "spin", "freeze",
    };
    static constexpr const char* kOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan, Voice Diffusion", nullptr, kInputIds },
        { "tracker", "Pitch Detector (mono sum)", StageKind::kFeedback,
          "autocorrelation-based tracker, holds last pitch through silence", nullptr, kTrackerIds },
        { "resonators", "6 Resonator Voices (3 left / 3 right)", StageKind::kFeedback,
          "own HarmonicInterval/Level/Pan/Duration/HiCut per voice, retuned each hop to a diatonic "
          "degree relative to the tracked note", nullptr, kResonatorIds },
        { "reverb", "Plate (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank",
          nullptr, kReverbIds },
        { "output", "Output", StageKind::kOutput, "FX Mix blends pre-reverb vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix",
          nullptr, kOutputIds },
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
