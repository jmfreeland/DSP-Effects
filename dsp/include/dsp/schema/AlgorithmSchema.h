#pragma once

#include <span>

namespace dsp::schema
{
/**
 * A lightweight, hand-authored description of a Block's signal-flow
 * topology - stages and the connections between them - for anything
 * that wants to *visualize* an algorithm without re-deriving its
 * structure from the C++ or the original manual each time: today, the
 * JUCE plugins' "Show Architecture" view; later, auto-rendered docs
 * diagrams or a generic cross-algorithm "Loom" plugin UI (see
 * docs/algorithm-schema-notes.md, which first proposed this - written
 * once 5 reverb cores existed to show real variety, not just one shape).
 *
 * Deliberately plain C++ (spans over static arrays), not an external
 * YAML/JSON file: it's consumed entirely by C++ (the JUCE plugin), and
 * this keeps it a single compiled, type-checked source of truth with no
 * parser dependency. Hand-authored *alongside* each Block, not generated
 * from it - if a Block's real topology and this description drift apart,
 * that's a bug in this file, not the engine.
 */
enum class StageKind
{
    kInput,       // where audio enters the diagram
    kProcessing,  // a linear processing stage (PreDelay, Diffusion, ...)
    kFeedback,    // a stage with its own internal feedback/recirculation (the tank)
    kOutput,      // where the diagram's result is produced
};

struct Stage
{
    const char* id;
    const char* label;
    StageKind kind;
    // Free-form detail shown alongside the stage (e.g. which control(s)
    // drive it) - may be nullptr.
    const char* detail = nullptr;
};

struct Connection
{
    const char* fromId;
    const char* toId;
    // Optional label on the arrow (e.g. "diffused", "* RvbOut") - may be
    // nullptr. A connection from a stage to itself is drawn as a
    // feedback loop rather than an arrow.
    const char* label = nullptr;
};

struct AlgorithmSchema
{
    const char* name;
    // One line describing what makes this algorithm's topology distinct
    // from the shared reverb core (empty for the reference core itself).
    const char* characterNote;
    std::span<const Stage> stages;
    std::span<const Connection> connections;
};
}
