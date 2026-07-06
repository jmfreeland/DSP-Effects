#pragma once

#include "dsp/schema/AlgorithmSchema.h"

namespace dsp::schema
{
/**
 * Hand-authored topology descriptions for the five PCM81 reverb cores
 * (see dsp/algorithms/ReverbCore.h and docs/lexicon-pcm81-*.md for the
 * engines these describe). Each mirrors the "Block topology" ASCII
 * diagram already in that algorithm's doc - this is the same structure,
 * meant for a UI to render rather than a person to read as prose.
 *
 * Two stages - Diffusion and the Tank - have identical internal
 * mechanisms across all five cores (only parameter *values* differ, per
 * ReverbCore.h being the shared base), so their drill-down detail is one
 * schema each (tankDetailSchema()/diffusionDetailSchema()), reused by
 * every core's "tank"/"diffusion" Stage rather than duplicated 5 times.
 */

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
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths", &diffusionDetailSchema() },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus",
          &tankDetailSchema() },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix" },
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
        "Concert Hall", "The reference core - every other core is this plus one addition.", stages,
        connections
    };
    return schema;
}

inline const AlgorithmSchema& plateSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing,
          "4x Allpass; Attack dips density for ~50ms on new onsets", &diffusionDetailSchema() },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus",
          &tankDetailSchema() },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix" },
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
        "Plate", "Concert Hall + a recirculating Pre-Echo + Attack-shaped Diffusion.", stages,
        connections
    };
    return schema;
}

inline const AlgorithmSchema& chamberSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths", &diffusionDetailSchema() },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus",
          &tankDetailSchema() },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix, Shape+Spread onset swell" },
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
        "Chamber", "Concert Hall + a recirculating Pre-Echo + a Shape/Spread onset-swell output envelope.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& infiniteSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths", &diffusionDetailSchema() },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback,
          "Householder mix, Rt HC damping, Spin/Chorus - Freeze bypasses damping too, holding near-losslessly",
          &tankDetailSchema() },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix, Shape+Spread onset swell" },
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
        "Infinite", "Chamber + Freeze: the tank's own damping is bypassed too, so a frozen tail holds "
                    "near-indefinitely instead of just decaying slower.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& inverseSchema()
{
    static const Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths", &diffusionDetailSchema() },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback,
          "Fixed generous internal sustain - Duration/Slope don't touch this, so there's always something to reveal",
          &tankDetailSchema() },
        { "output", "Output", StageKind::kOutput,
          "Re-split into Low/Mid bands, each shaped by its own Duration+Slope envelope (decay, gate, or rise), "
          "then hard-cut at Duration; also swelled by Shape (Spread fixed for this algorithm)" },
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
        "Inverse", "Concert Hall's tank kept healthy internally, but RT60 decay replaced entirely by a "
                   "read-path-only Duration+Slope envelope, independent per band.",
        stages, connections
    };
    return schema;
}
}
