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
 */

inline const AlgorithmSchema& concertHallSchema()
{
    static constexpr Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths" },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus" },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix" },
    };
    static constexpr Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band (Link: scales w/ Size)" },
        { "tank", "output", "* RvbOut" },
    };
    static constexpr AlgorithmSchema schema = {
        "Concert Hall", "The reference core - every other core is this plus one addition.", stages,
        connections
    };
    return schema;
}

inline const AlgorithmSchema& plateSchema()
{
    static constexpr Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass; Attack dips density for ~50ms on new onsets" },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus" },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix" },
    };
    static constexpr Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preEcho", "raw L/R" },
        { "preEcho", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band" },
        { "tank", "output", "* RvbOut" },
    };
    static constexpr AlgorithmSchema schema = {
        "Plate", "Concert Hall + a recirculating Pre-Echo + Attack-shaped Diffusion.", stages,
        connections
    };
    return schema;
}

inline const AlgorithmSchema& chamberSchema()
{
    static constexpr Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths" },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback, "Householder mix, Rt HC damping, Spin/Chorus" },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix, Shape+Spread onset swell" },
    };
    static constexpr Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preEcho", "raw L/R" },
        { "preEcho", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band" },
        { "tank", "output", "* RvbOut, then Shape+Spread envelope" },
    };
    static constexpr AlgorithmSchema schema = {
        "Chamber", "Concert Hall + a recirculating Pre-Echo + a Shape/Spread onset-swell output envelope.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& infiniteSchema()
{
    static constexpr Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preEcho", "Pre-Echo", StageKind::kProcessing, "EkoDly/EkoFbk, recirculating Comb per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths" },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback,
          "Householder mix, Rt HC damping, Spin/Chorus - Freeze bypasses damping too, holding near-losslessly" },
        { "output", "Output", StageKind::kOutput, "Depth blend, Mix, Shape+Spread onset swell" },
    };
    static constexpr Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preEcho", "raw L/R" },
        { "preEcho", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "Low Rt/Mid Rt per band, or ~0.9999/no damping while frozen" },
        { "tank", "output", "* RvbOut, then Shape+Spread envelope" },
    };
    static constexpr AlgorithmSchema schema = {
        "Infinite", "Chamber + Freeze: the tank's own damping is bypassed too, so a frozen tail holds "
                    "near-indefinitely instead of just decaying slower.",
        stages, connections
    };
    return schema;
}

inline const AlgorithmSchema& inverseSchema()
{
    static constexpr Stage stages[] = {
        { "input", "Input L/R", StageKind::kInput },
        { "earlyReflections", "Early Reflections", StageKind::kProcessing, "RefDly/RefLvl per channel" },
        { "preDelay", "PreDelay", StageKind::kProcessing, "0-930ms" },
        { "diffusion", "Diffusion", StageKind::kProcessing, "4x Allpass, prime lengths" },
        { "tank", "8-Line FDN Tank", StageKind::kFeedback,
          "Fixed generous internal sustain - Duration/Slope don't touch this, so there's always something to reveal" },
        { "output", "Output", StageKind::kOutput,
          "Re-split into Low/Mid bands, each shaped by its own Duration+Slope envelope (decay, gate, or rise), "
          "then hard-cut at Duration" },
    };
    static constexpr Connection connections[] = {
        { "input", "earlyReflections", nullptr },
        { "earlyReflections", "output", "parallel, Depth-blended" },
        { "input", "preDelay", "mono sum * RvbIn" },
        { "preDelay", "diffusion", nullptr },
        { "diffusion", "tank", "diffused" },
        { "tank", "tank", "fixed sustain gain (~2.5s) - not user-adjustable" },
        { "tank", "output", "* RvbOut, then band-split + Low Slope/Mid Slope" },
    };
    static constexpr AlgorithmSchema schema = {
        "Inverse", "Concert Hall's tank kept healthy internally, but RT60 decay replaced entirely by a "
                   "read-path-only Duration+Slope envelope, independent per band.",
        stages, connections
    };
    return schema;
}
}
