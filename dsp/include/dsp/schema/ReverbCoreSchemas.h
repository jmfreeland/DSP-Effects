#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
/**
 * Hand-authored topology descriptions for the five PCM81 reverb cores
 * (see dsp/algorithms/ReverbCore.h and docs/lexicon-pcm81-*.md for the
 * engines these describe), plus, for each, the full *algorithm* schema
 * the plugin actually shows as its root: the manual's own 4-Voice
 * "Reverb Shell" (input conditioning, a parallel 4-voice delay bank,
 * post delay, and the shared FX output chain) wrapped around that core,
 * which stays reachable as a drill-down - matching the manual's
 * algorithm pages, which draw the voices and the reverb block together
 * in one diagram.
 *
 * Two stages - Diffusion and the Tank - have identical internal
 * mechanisms across all five cores (only parameter *values* differ, per
 * ReverbCore.h being the shared base), so their drill-down detail is one
 * schema each (tankDetailSchema()/diffusionDetailSchema()), reused by
 * every core's "tank"/"diffusion" Stage rather than duplicated 5 times.
 * The same sharing applies to the shell's parameter IDs (detail::
 * kShell*Ids below): all five plugins register the identical shell
 * parameter set, so one set of arrays serves every shell schema.
 *
 * Parameter IDs are as authored in each plugin's PluginProcessor -
 * validated against the live parameter list in debug builds by
 * LoomParametersPanel.
 */

namespace detail
{
// The 4-Voice Reverb Shell's own parameters, identical across all five
// reverb-core plugins.
inline constexpr const char* kShellInputIds[] = {
    "inLevelLeft", "inLevelRight", "inPanLeft", "inPanRight",
};
inline constexpr const char* kShellVoiceIds[] = {
    "voiceDiffusion",
    "voice0Delay", "voice0Feedback", "voice0Level", "voice0Pan",
    "voice1Delay", "voice1Feedback", "voice1Level", "voice1Pan",
    "voice2Delay", "voice2Feedback", "voice2Level", "voice2Pan",
    "voice3Delay", "voice3Feedback", "voice3Level", "voice3Pan",
    "voiceGlideResponse", "voiceGlideRange", "clear",
};
inline constexpr const char* kShellCoreIds[] = { "rvbWidth" };
inline constexpr const char* kShellPostDelayIds[] = {
    "postDelayLeft", "postDelayRight", "postDelayGlideResponse", "postDelayGlideRange",
    "postDelayMix",
};
inline constexpr const char* kShellOutputIds[] = { "fxMix", "fxWidth", "hiCut", "fxAdjust", "mix" };

// Core-stage parameter groups shared verbatim by several cores.
inline constexpr const char* kEarlyReflectionIds[] = {
    "earlyReflectionLevelLeft", "earlyReflectionLevelRight",
    "earlyReflectionDelayLeft", "earlyReflectionDelayRight",
};
inline constexpr const char* kPreEchoIds[] = {
    "ekoDelayLeft", "ekoDelayRight", "ekoFeedbackLeft", "ekoFeedbackRight",
};
inline constexpr const char* kPreDelayIds[] = { "rvbIn", "preDelay" };
inline constexpr const char* kDiffusionIds[] = { "diffusion" };
inline constexpr const char* kTankIds[] = {
    "decay", "lowRatio", "crossover", "damping", "size",
    "link",  "definition", "spin",   "chorus",  "freeze",
};
inline constexpr const char* kCoreOutputIds[] = { "depth", "rvbOut" };
}

inline const AlgorithmSchema& diffusionDetailSchema()
{
    static const Stage stages[] = {
        { "in", "Input (mono)", StageKind::kInput },
        { "stage1", "Allpass 1", StageKind::kProcessing, "211 samples, ~4.4ms @ 48kHz" },
        { "stage2", "Allpass 2", StageKind::kProcessing, "431 samples, ~9.0ms" },
        { "stage3", "Allpass 3", StageKind::kProcessing, "751 samples, ~15.7ms" },
        { "stage4", "Allpass 4", StageKind::kProcessing, "1091 samples, ~22.7ms" },
        { "out", "Diffused Output", StageKind::kOutput, "-> tank input" },
    };
    static const Connection connections[] = {
        { "in", "stage1", nullptr },
        { "stage1", "stage2", nullptr },
        { "stage2", "stage3", nullptr },
        { "stage3", "stage4", nullptr },
        { "stage4", "out", nullptr },
    };
    static const AlgorithmSchema schema = {
        "Diffusion (inside every core)",
        "4 series Allpass stages sharing one Diffusion coefficient (0 = clear, 1 = maximum smear) - "
        "prime/coprime lengths so the resulting echo cluster doesn't itself ring periodically. "
        "Plate's Attack modulates the coefficient dynamically here for the first ~50ms after an onset.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& tankDetailSchema()
{
    static const Stage stages[] = {
        { "tap", "Delay Read (x8 lines)", StageKind::kInput,
          "length_i * Size +/- Spin/Chorus wobble (fractional-interpolated)" },
        { "damping", "Rt HC Damping", StageKind::kProcessing, "OnePoleLowpass per line" },
        { "crossover", "Low/Mid Split", StageKind::kProcessing, "Crossover at a settable Hz" },
        { "decayGain", "Decay Gain", StageKind::kProcessing,
          "Per-line, per-band - RT60 (Low Rt/Mid Rt) for Hall/Plate/Chamber/Infinite; fixed "
          "sustain for Inverse (its Duration/Slope shape the output instead, not this)" },
        { "mix", "Householder Mix", StageKind::kFeedback,
          "8x8 lossless reflection matrix - redistributes energy across all lines, doesn't add or remove it" },
        { "write", "Write Back", StageKind::kOutput, "diffused input * sign_i + decayed value" },
    };
    static const Connection connections[] = {
        { "tap", "damping", nullptr },
        { "damping", "crossover", nullptr },
        { "crossover", "decayGain", "low + high bands" },
        { "decayGain", "mix", "all 8 lines" },
        { "mix", "write", nullptr },
        { "write", "tap", "one line-length later" },
    };
    static const AlgorithmSchema schema = {
        "8-Line FDN Tank (inside every core)",
        "The recirculating heart of every reverb core here: 8 mutually-coprime delay lines, damped, "
        "band-split, decayed, and losslessly remixed every sample. What differs per core is only "
        "the Decay Gain stage's formula (and, for Plate/Chamber/Infinite, an added Pre-Echo ahead "
        "of this tank, not shown here - see the parent diagram).",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& concertHallSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel",
          nullptr, detail::kEarlyReflectionIds },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms", nullptr, detail::kPreDelayIds },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths",
          &diffusionDetailSchema(), detail::kDiffusionIds },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus",
          &tankDetailSchema(), detail::kTankIds },
        { "output", "Core Output", StageKind::kOutput, "Depth blend", nullptr, detail::kCoreOutputIds },
    };
    static const Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band (Link: scales w/ Size)" },
        { "tank", "output", "* RvbOut" },
    };
    static const AlgorithmSchema schema = {
        "Concert Hall Core", "The reference core - every other core is this plus one addition.", stages,
        connections
    };
    return schema;
}

inline const AlgorithmSchema& plateSchema()
{
    static constexpr const char* kPlateDiffusionIds[] = { "diffusion", "attack" };
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel",
          nullptr, detail::kEarlyReflectionIds },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel",
          nullptr, detail::kPreEchoIds },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms", nullptr, detail::kPreDelayIds },
        { "diffusion", "Diffusion", StageKind::kProcessing,
          "4x Allpass; Attack dips density for ~50ms on new onsets", &diffusionDetailSchema(),
          kPlateDiffusionIds },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus",
          &tankDetailSchema(), detail::kTankIds },
        { "output", "Core Output", StageKind::kOutput, "Depth blend", nullptr, detail::kCoreOutputIds },
    };
    static const Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preEcho", "raw L/R" },
        { "preEcho", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band" },
        { "tank", "output", "* RvbOut" },
    };
    static const AlgorithmSchema schema = {
        "Plate Core", "Concert Hall + a recirculating Pre-Echo + Attack-shaped Diffusion.", stages,
        connections
    };
    return schema;
}

inline const AlgorithmSchema& chamberSchema()
{
    static constexpr const char* kChamberOutputIds[] = { "depth", "rvbOut", "shape", "spread" };
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel",
          nullptr, detail::kEarlyReflectionIds },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel",
          nullptr, detail::kPreEchoIds },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms", nullptr, detail::kPreDelayIds },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths",
          &diffusionDetailSchema(), detail::kDiffusionIds },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus",
          &tankDetailSchema(), detail::kTankIds },
        { "output", "Core Output", StageKind::kOutput, "Depth blend, Shape+Spread onset swell", nullptr,
          kChamberOutputIds },
    };
    static const Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preEcho", "raw L/R" },
        { "preEcho", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band" },
        { "tank", "output", "* RvbOut, then Shape+Spread envelope" },
    };
    static const AlgorithmSchema schema = {
        "Chamber Core",
        "Concert Hall + a recirculating Pre-Echo + a Shape/Spread onset-swell output envelope.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& infiniteSchema()
{
    static constexpr const char* kInfiniteOutputIds[] = { "depth", "rvbOut", "shape", "spread" };
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel",
          nullptr, detail::kEarlyReflectionIds },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel",
          nullptr, detail::kPreEchoIds },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms", nullptr, detail::kPreDelayIds },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths",
          &diffusionDetailSchema(), detail::kDiffusionIds },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback,
          "Householder mix, Rt HC damping, Spin/Chorus - Freeze bypasses damping too, holding near-losslessly",
          &tankDetailSchema(), detail::kTankIds },
        { "output", "Core Output", StageKind::kOutput, "Depth blend, Shape+Spread onset swell", nullptr,
          kInfiniteOutputIds },
    };
    static const Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preEcho", "raw L/R" },
        { "preEcho", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band, or ~0.9999/no damping while frozen" },
        { "tank", "output", "* RvbOut, then Shape+Spread envelope" },
    };
    static const AlgorithmSchema schema = {
        "Infinite Core", "Chamber + Freeze: the tank's own damping is bypassed too, so a frozen tail holds "
                         "near-indefinitely instead of just decaying slower.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& inverseSchema()
{
    static constexpr const char* kInverseTankIds[] = {
        "crossover", "damping", "size", "definition", "spin", "chorus", "freeze",
    };
    static constexpr const char* kInverseOutputIds[] = {
        "depth", "rvbOut", "duration", "lowSlope", "midSlope", "shape",
    };
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel",
          nullptr, detail::kEarlyReflectionIds },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms", nullptr, detail::kPreDelayIds },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths",
          &diffusionDetailSchema(), detail::kDiffusionIds },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback,
          "Fixed generous internal sustain - Duration/Slope don't touch this, so there's always something to reveal",
          &tankDetailSchema(), kInverseTankIds },
        { "output", "Core Output", StageKind::kOutput,
          "Re-split into Low/Mid bands, each shaped by its own Duration+Slope envelope (decay, gate, or rise), "
          "then hard-cut at Duration; also swelled by Shape (Spread fixed for this algorithm)", nullptr,
          kInverseOutputIds },
    };
    static const Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "fixed sustain gain (~2.5s) - not user-adjustable" },
        { "tank", "output", "* RvbOut, then band-split + Low Slope/Mid Slope, then Shape swell" },
    };
    static const AlgorithmSchema schema = {
        "Inverse Core", "Concert Hall's tank kept healthy internally, but RT60 decay replaced entirely by a "
                        "read-path-only Duration+Slope envelope, independent per band.",
        stages, connections
    };
    return schema;
}

// The shared shell topology is identical for all five algorithms, so the
// connections are one static array; only each shell's core stage (label,
// note, drill-down target) and top-level name differ.
namespace detail
{
inline constexpr Connection kShellConnections[] = {
    { "input", "voices", "mono sum" },
    { "input", "core", "stereo" },
    { "voices", "output", "FX Mix = 0" },
    { "core", "output", "FX Mix = 1" },
    { "core", "postDelay", nullptr },
    { "postDelay", "output", "Post Mix" },
};
}

inline const AlgorithmSchema& concertHallAlgorithmSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan conditioning", nullptr,
          detail::kShellInputIds },
        { "voices", "4-Voice Bank", StageKind::kProcessing,
          "4 parallel delay voices (own Delay/Feedback/Level/Pan) behind a shared Voice Diffusion, "
          "fed from the mono sum; delay changes glide per GldResp/GldRange",
          nullptr, detail::kShellVoiceIds },
        { "core", "Concert Hall Core", StageKind::kFeedback,
          "The shared reverb core; Rvb Width rotates its stereo output", &concertHallSchema(),
          detail::kShellCoreIds },
        { "postDelay", "Post Delay", StageKind::kProcessing,
          "Independent L/R delay taps on the reverb output, glided, blended in by Post Mix",
          nullptr, detail::kShellPostDelayIds },
        { "output", "Output", StageKind::kOutput, "FX Mix (voices vs. reverb), FX Width/Hi-Cut/Adjust, Mix",
          nullptr, detail::kShellOutputIds },
    };
    static const AlgorithmSchema schema = {
        "Concert Hall",
        "The PCM81's 4-Voice Reverb Shell around the Concert Hall core - click the core to see inside it.",
        stages, detail::kShellConnections
    };
    return schema;
}

inline const AlgorithmSchema& plateAlgorithmSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan conditioning", nullptr,
          detail::kShellInputIds },
        { "voices", "4-Voice Bank", StageKind::kProcessing,
          "4 parallel delay voices (own Delay/Feedback/Level/Pan) behind a shared Voice Diffusion, "
          "fed from the mono sum; delay changes glide per GldResp/GldRange",
          nullptr, detail::kShellVoiceIds },
        { "core", "Plate Core", StageKind::kFeedback,
          "The shared reverb core with Pre-Echo and Attack; Rvb Width rotates its stereo output",
          &plateSchema(), detail::kShellCoreIds },
        { "postDelay", "Post Delay", StageKind::kProcessing,
          "Independent L/R delay taps on the reverb output, glided, blended in by Post Mix",
          nullptr, detail::kShellPostDelayIds },
        { "output", "Output", StageKind::kOutput, "FX Mix (voices vs. reverb), FX Width/Hi-Cut/Adjust, Mix",
          nullptr, detail::kShellOutputIds },
    };
    static const AlgorithmSchema schema = {
        "Plate",
        "The PCM81's 4-Voice Reverb Shell around the Plate core - click the core to see inside it.",
        stages, detail::kShellConnections
    };
    return schema;
}

inline const AlgorithmSchema& chamberAlgorithmSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan conditioning", nullptr,
          detail::kShellInputIds },
        { "voices", "4-Voice Bank", StageKind::kProcessing,
          "4 parallel delay voices (own Delay/Feedback/Level/Pan) behind a shared Voice Diffusion, "
          "fed from the mono sum; delay changes glide per GldResp/GldRange",
          nullptr, detail::kShellVoiceIds },
        { "core", "Chamber Core", StageKind::kFeedback,
          "The shared reverb core with Pre-Echo and Shape/Spread; Rvb Width rotates its stereo output",
          &chamberSchema(), detail::kShellCoreIds },
        { "postDelay", "Post Delay", StageKind::kProcessing,
          "Independent L/R delay taps on the reverb output, glided, blended in by Post Mix",
          nullptr, detail::kShellPostDelayIds },
        { "output", "Output", StageKind::kOutput, "FX Mix (voices vs. reverb), FX Width/Hi-Cut/Adjust, Mix",
          nullptr, detail::kShellOutputIds },
    };
    static const AlgorithmSchema schema = {
        "Chamber",
        "The PCM81's 4-Voice Reverb Shell around the Chamber core - click the core to see inside it.",
        stages, detail::kShellConnections
    };
    return schema;
}

inline const AlgorithmSchema& infiniteAlgorithmSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan conditioning", nullptr,
          detail::kShellInputIds },
        { "voices", "4-Voice Bank", StageKind::kProcessing,
          "4 parallel delay voices (own Delay/Feedback/Level/Pan) behind a shared Voice Diffusion, "
          "fed from the mono sum; delay changes glide per GldResp/GldRange",
          nullptr, detail::kShellVoiceIds },
        { "core", "Infinite Core", StageKind::kFeedback,
          "Chamber's core with Freeze holding the tail near-losslessly; Rvb Width rotates its output",
          &infiniteSchema(), detail::kShellCoreIds },
        { "postDelay", "Post Delay", StageKind::kProcessing,
          "Independent L/R delay taps on the reverb output, glided, blended in by Post Mix",
          nullptr, detail::kShellPostDelayIds },
        { "output", "Output", StageKind::kOutput, "FX Mix (voices vs. reverb), FX Width/Hi-Cut/Adjust, Mix",
          nullptr, detail::kShellOutputIds },
    };
    static const AlgorithmSchema schema = {
        "Infinite",
        "The PCM81's 4-Voice Reverb Shell around the Infinite core - click the core to see inside it.",
        stages, detail::kShellConnections
    };
    return schema;
}

inline const AlgorithmSchema& inverseAlgorithmSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput, "InLvl/InPan conditioning", nullptr,
          detail::kShellInputIds },
        { "voices", "4-Voice Bank", StageKind::kProcessing,
          "4 parallel delay voices (own Delay/Feedback/Level/Pan) behind a shared Voice Diffusion, "
          "fed from the mono sum; delay changes glide per GldResp/GldRange",
          nullptr, detail::kShellVoiceIds },
        { "core", "Inverse Core", StageKind::kFeedback,
          "The envelope-shaped core (Duration/Slopes, no RT60); Rvb Width rotates its stereo output",
          &inverseSchema(), detail::kShellCoreIds },
        { "postDelay", "Post Delay", StageKind::kProcessing,
          "Independent L/R delay taps on the reverb output, glided, blended in by Post Mix",
          nullptr, detail::kShellPostDelayIds },
        { "output", "Output", StageKind::kOutput, "FX Mix (voices vs. reverb), FX Width/Hi-Cut/Adjust, Mix",
          nullptr, detail::kShellOutputIds },
    };
    static const AlgorithmSchema schema = {
        "Inverse",
        "The PCM81's 4-Voice Reverb Shell around the Inverse core - click the core to see inside it.",
        stages, detail::kShellConnections
    };
    return schema;
}
}
