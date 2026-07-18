#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& glideHallSchema()
{
    // Parameter IDs as authored in plugin/source/GlideHallPluginProcessor.cpp,
    // validated against the live parameter list in debug builds.
    static constexpr const char* kInputIds[] = {
        "inLevelLeft", "inLevelRight", "inPanLeft", "inPanRight", "voiceDiffusion",
    };
    static constexpr const char* kGlideTapIds[] = {
        "glideLevel",
        "glideTapALevelLeft", "glideTapADelayLeft", "glideTapALevelRight", "glideTapADelayRight",
        "glideTapBLevelLeft", "glideTapBDelayLeft", "glideTapBLevelRight", "glideTapBDelayRight",
        "glideFeedbackLeft", "glideFeedbackRight", "glideCrossFeedbackLeft", "glideCrossFeedbackRight",
    };
    static constexpr const char* kVoiceBankIds[] = {
        "voice0Delay", "voice0Level", "voice0Pan", "voice0Feedback", "voice0CrossFeedback",
        "voice1Delay", "voice1Level", "voice1Pan", "voice1Feedback", "voice1CrossFeedback",
        "voice2Delay", "voice2Level", "voice2Pan", "voice2Feedback", "voice2CrossFeedback",
        "voice3Delay", "voice3Level", "voice3Pan", "voice3Feedback", "voice3CrossFeedback",
        "voice4Delay", "voice4Level", "voice4Pan", "voice4Feedback", "voice4CrossFeedback",
        "voice5Delay", "voice5Level", "voice5Pan", "voice5Feedback", "voice5CrossFeedback",
    };
    static constexpr const char* kReverbIds[] = {
        "decay", "lowRatio", "crossover", "damping", "diffusion", "size", "link",
        "definition", "depth", "rvbIn", "rvbOut", "preDelay",
        "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
        "earlyReflectionDelayLeft", "earlyReflectionDelayRight", "spin", "chorus", "freeze",
    };
    static constexpr const char* kOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan, Voice Diffusion", nullptr, kInputIds },
        { "glideTaps", "Glide Delay (A/B x L/R)", StageKind::kFeedback,
          "0-42ms per tap, GlideParameter'd for flange/pitch-mod character", nullptr, kGlideTapIds },
        { "voiceBank", "6-Voice Bank (3 left / 3 right)", StageKind::kFeedback,
          "own Delay/Level/Pan/Feedback/Cross-Feedback per voice", nullptr, kVoiceBankIds },
        { "reverb", "Concert Hall (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank",
          nullptr, kReverbIds },
        { "output", "Output", StageKind::kOutput, "FX Mix blends pre-reverb vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix",
          nullptr, kOutputIds },
    };
    static const Connection connections[] = {
        { "input", "glideTaps", nullptr },
        { "glideTaps", "glideTaps", "Fbk L/R (own channel), X-Fbk L/R (cross channel)" },
        { "glideTaps", "voiceBank", nullptr, "Gld Lvl" },
        { "voiceBank", "voiceBank", "Voice Fbk (own bank), Voice X-Fbk (cross bank)" },
        { "voiceBank", "reverb", "series - the six-voice output becomes the reverb's input" },
        { "voiceBank", "output", "FX Mix = 0" },
        { "reverb", "output", "FX Mix = 1" },
    };
    static const AlgorithmSchema schema = {
        "Glide>Hall",
        "The first of the manual's five 6-Voice algorithms: a stereo pair of gliding delay taps "
        "feeds six delay voices (three fed from the left glide output, three from the right, each "
        "with its own Delay/Level/Pan/Feedback/Cross-Feedback), whose combined output runs in "
        "series into a fixed Concert Hall reverb - unlike Chorus+Rvb and M-Band+Rvb, which run "
        "their 6-voice effect in parallel with the reverb instead.",
        stages, connections
    };
    return schema;
}
}
