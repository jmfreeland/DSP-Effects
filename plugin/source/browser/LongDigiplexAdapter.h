#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/LongDigiplexAlgorithm.h"
#include "dsp/schema/LongDigiplexSchema.h"

#include <span>

// Adapts dsp::graphs::LongDigiplexAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// LongDigiplexPluginProcessor.cpp's own layout/processBlock, namespaced
// under "longDigiplex" (Algorithm 109). The simplest H3000 Block yet: a
// single DelayLine with feedback and glide-smoothed delay changes.
namespace loom::browser
{
class LongDigiplexAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "longDigiplex"; }
    const char* displayName() const override { return "Eventide Long Digiplex"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("longDigiplex", suffix); };

        params.push_back(floatParam(pid("delay"), "Delay", 0.0f, 1.4f, 0.3f, "s"));
        params.push_back(floatParam(pid("feedback"), "Feedback", -1.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("glideResponse"), "Glide Speed", 0.0f, 100.0f, 50.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("glideEnabled"), 1 }, "Glide Enabled", true));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("repeat"), 1 }, "Repeat", false));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("inLevel"), "In Level", -1.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::longDigiplexSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::LongDigiplexAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("longDigiplex", suffix)); };

        engine_.setGlide(v("glideResponse"), v("glideEnabled") >= 0.5f);
        engine_.setDelaySeconds(v("delay"));
        engine_.setFeedback(v("feedback"));
        engine_.setRepeat(v("repeat") >= 0.5f);
        engine_.setMix(v("mix"));
        engine_.setInLevel(v("inLevel"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::LongDigiplexAlgorithm engine_;
};
}
