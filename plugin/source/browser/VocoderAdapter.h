#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/VocoderAlgorithm.h"
#include "dsp/schema/VocoderSchema.h"

#include <span>

// Adapts dsp::graphs::VocoderAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// VocoderPluginProcessor.cpp's own layout/processBlock, namespaced under
// "vocoder" (Algorithm 115). Left = synthesis (instrument), Right =
// analysis (voice), per the manual - the first algorithm in this archive
// where Left/Right are channel-specific rather than interchangeable.
namespace loom::browser
{
class VocoderAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "vocoder"; }
    const char* displayName() const override { return "Eventide Vocoder"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("vocoder", suffix); };

        params.push_back(floatParam(pid("formantSpeed"), "Formant Speed", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("envelopeSpeed"), "Envelope Speed", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("formantShift"), "Formant Shift", 0.0f, 100.0f, 0.0f));
        params.push_back(floatParam(pid("depth"), "Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("width"), "Width", 0.0f, 0.01f, 0.005f, "s"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("maxResonance"), "Max Resonance", 0.0f, 100.0f, 30.0f));
        params.push_back(floatParam(pid("threshold"), "Threshold", 0.0f, 1.0f, 0.02f));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::vocoderSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::VocoderAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("vocoder", suffix)); };

        engine_.setFormantSpeed(v("formantSpeed"));
        engine_.setEnvelopeSpeed(v("envelopeSpeed"));
        engine_.setFormantShift(v("formantShift"));
        engine_.setDepth(v("depth"));
        engine_.setWidthSeconds(v("width"));
        engine_.setMix(v("mix"));
        engine_.setMaxResonance(v("maxResonance"));
        engine_.setThreshold(v("threshold"));
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::VocoderAlgorithm engine_;
};
}
