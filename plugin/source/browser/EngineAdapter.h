#pragma once

#include "dsp/schema/AlgorithmSchema.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

// The common surface every algorithm registered with the Loom browser
// plugin exposes, so the browser can own exactly one live engine at a
// time behind a single interface rather than special-casing dozens of
// bespoke Graphs. Each adapter is the same parameter-layout-plus-
// processBlock logic every single-algorithm PluginProcessor already has
// (see e.g. PluginProcessor.cpp for Concert Hall), just split so the
// browser can call the pieces independently: parameters are created
// once - with ids namespaced by this adapter's own id() prefix, so
// identically-named controls across algorithms ("decay", "mix") don't
// collide in the one shared APVTS every algorithm's parameters live in
// - while the engine itself is constructed only when this algorithm
// becomes the active selection (see LoomBrowserPluginProcessor).
namespace loom::browser
{
class EngineAdapter
{
  public:
    virtual ~EngineAdapter() = default;

    // Stable identifier, also used as the APVTS parameter-id prefix -
    // keep it a valid id fragment (alphanumeric, no spaces/punctuation).
    virtual const char* id() const = 0;

    // Shown in the algorithm picker.
    virtual const char* displayName() const = 0;

    // This algorithm's own parameters, with every id already prefixed
    // by id() (e.g. "concertHall_decay") - appended into the browser's
    // one shared APVTS alongside every other registered algorithm's.
    virtual std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const = 0;

    // The schema LoomBrowserPluginEditor renders while this algorithm
    // is selected. Its Stage::parameterIds are the same *unprefixed*
    // ids every single-algorithm plugin's schema already uses - the
    // editor is told this adapter's id() separately and prepends it
    // when resolving a parameter, so schemas need no changes to be
    // reused here.
    virtual const dsp::schema::AlgorithmSchema& schema() const = 0;

    virtual std::size_t requiredWorkingBufferSize() const = 0;

    // Constructs/resets this algorithm's engine into workingBuffer,
    // which must stay valid and exactly this size for as long as this
    // adapter remains the active selection.
    virtual void prepare(float sampleRate, std::span<float> workingBuffer) = 0;

    // Reads this adapter's own (prefixed) parameters from apvts, pushes
    // them into the engine, and processes one block - mirrors a single-
    // algorithm PluginProcessor::processBlock() exactly, just behind an
    // interface so the browser doesn't need to know which engine it's
    // driving.
    virtual void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                         std::span<float> right) = 0;
};
}
