#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& pitchCorrectSchema()
{
    // Parameter IDs as authored in plugin/source/PitchCorrectPluginProcessor.cpp,
    // validated against the live parameter list in debug builds.
    static constexpr const char* kInputIds[] = { "inLevelLeft", "inLevelRight" };
    static constexpr const char* kCorrectIds[] = {
        "delay",    "lowPitch", "highPitch",  "tuning",         "correction",
        "tracking", "grain",    "shiftCents", "shiftSemitones",
    };
    static constexpr const char* kReverbIds[] = {
        "decay",   "lowRatio", "crossover", "damping", "diffusion",
        "size",    "link",     "shape",     "spread",  "rvbIn",
        "rvbOut",  "preDelay", "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight", "spin",
        "ekoFeedbackLeft", "ekoFeedbackRight", "ekoDelayLeft", "ekoDelayRight",
    };
    static constexpr const char* kOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput,
          "InLvl only, no InPan - internally mono, see doc comment", nullptr, kInputIds },
        { "correct", "Pitch Correct", StageKind::kFeedback,
          "PitchDetector -> nearest chromatic semitone (Tuning ref) -> PitchShifter; Correction blends the amount",
          nullptr, kCorrectIds },
        { "reverb", "Chamber (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank",
          nullptr, kReverbIds },
        { "output", "Output", StageKind::kOutput,
          "FX Mix blends dry-corrected vs. reverbed signal (default near 0); FX Width/Hi-Cut/Adjust/Mix",
          nullptr, kOutputIds },
    };
    static const Connection connections[] = {
        { "input", "correct", "mono sum" },
        { "correct", "reverb", "series - the corrected signal becomes the reverb's input" },
        { "correct", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "Pitch Correct",
        "The seventh and last Pitch algorithm, completing the PCM81's full 17-algorithm roadmap: "
        "\"designed to work with monophonic (one note at a time) vocal sources... an intelligent "
        "pitch shifter [that] detects the pitch of incoming audio and produces corrections based on "
        "the detected pitch,\" combined with a fixed Chamber reverb in series. Unlike Res2>Plate's "
        "diatonic Key/Scale/Root harmonizer, this algorithm's own edit matrix has no Key/Scale/Root "
        "row at all - it corrects toward the nearest chromatic (equal-tempered) semitone relative to "
        "the Tuning reference, a simpler 12-TET quantizer rather than a diatonic-degree lookup.",
        stages, connections
    };
    return schema;
}
}
