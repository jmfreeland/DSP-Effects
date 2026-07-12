#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& diatonicShiftSchema()
{
    static constexpr const char* kInputIds[] = { "inLevelLeft", "inLevelRight" };
    static constexpr const char* kDelayIds[] = { "delay", "grain" };
    static constexpr const char* kTrackerIds[] = { "key", "scale", "tune", "lowNoteHz", "highNoteHz" };
    static constexpr const char* kLeftVoiceIds[] = { "leftVoice", "leftFeedback", "leftMix" };
    static constexpr const char* kRightVoiceIds[] = { "rightVoice", "rightFeedback", "rightMix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "Summed to mono", nullptr, kInputIds },
        { "delay", "Delay", StageKind::kProcessing, "0-1s, shared ahead of tracking and both Voices", nullptr, kDelayIds },
        { "tracker", "Pitch Tracker", StageKind::kProcessing,
          "Real-time monophonic detection (autocorrelation) - drives both Voices' shift amount", nullptr, kTrackerIds },
        { "leftVoice", "Left Voice", StageKind::kProcessing,
          "Pitch Shifter, interval relative to the tracked note's own scale degree", nullptr, kLeftVoiceIds },
        { "rightVoice", "Right Voice", StageKind::kProcessing,
          "Pitch Shifter, interval relative to the tracked note's own scale degree", nullptr, kRightVoiceIds },
        { "output", "Output", StageKind::kOutput, "L/R Mix blends each Voice against dry" },
    };
    // Genuinely fan-out/fan-in (one delayed signal feeds three parallel
    // consumers; both Voices independently reach Output) rather than the
    // simpler linear-with-occasional-skip shape the other schemas have -
    // this is a busier diagram than those, honestly reflecting a busier
    // topology rather than trading accuracy for tidiness.
    static const Connection connections[] = {
        { "input", "delay", nullptr },
        { "delay", "tracker", "tapped" },
        { "delay", "leftVoice", "tapped" },
        { "delay", "rightVoice", "tapped" },
        { "tracker", "leftVoice", "sets shift amount" },
        { "tracker", "rightVoice", "sets shift amount" },
        { "leftVoice", "output", nullptr },
        { "rightVoice", "output", nullptr },
        { "leftVoice", "input", "* L Feedback - cascades another lap" },
        { "rightVoice", "input", "* R Feedback - cascades another lap" },
    };
    static const AlgorithmSchema schema = {
        "Diatonic Shift",
        "Both Voices shift the same tracked-and-delayed mono signal, each by its own harmonic "
        "interval computed relative to whichever scale degree the input note actually lands on - "
        "not a fixed transposition - so a 'third up' is the correct number of semitones whether "
        "the note played is the root or the 2nd degree.",
        stages, connections
    };
    return schema;
}
}
