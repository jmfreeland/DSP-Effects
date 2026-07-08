#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
inline const AlgorithmSchema& glideHallSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan, Voice Diffusion" },
        { "glideTaps", "Glide Delay (A/B x L/R)", StageKind::kFeedback,
          "0-42ms per tap, GlideParameter'd for flange/pitch-mod character" },
        { "voiceBank", "6-Voice Bank (3 left / 3 right)", StageKind::kFeedback,
          "own Delay/Level/Pan/Feedback/Cross-Feedback per voice" },
        { "reverb", "Concert Hall (fixed, in series)", StageKind::kFeedback, "8-line Householder FDN tank" },
        { "output", "Output", StageKind::kOutput, "FX Mix blends pre-reverb vs. reverbed signal; FX Width/Hi-Cut/Adjust/Mix" },
    };
    static const Connection connections[] = {
        { "input", "glideTaps", nullptr },
        { "glideTaps", "glideTaps", "Fbk L/R (own channel), X-Fbk L/R (cross channel)" },
        { "glideTaps", "voiceBank", "* Gld Lvl" },
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
