#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/TimesqueezeAlgorithm.h"
#include "dsp/schema/TimesqueezeSchema.h"

#include <span>

// Adapts dsp::graphs::TimesqueezeAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// TimesqueezePluginProcessor.cpp's own layout/processBlock, namespaced
// under "timesqueeze" (Algorithm 113).
namespace loom::browser
{
class TimesqueezeAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "timesqueeze"; }
    const char* displayName() const override { return "Eventide Timesqueeze"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("timesqueeze", suffix); };

        params.push_back(floatParam(pid("time"), "Time", -87.5f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("pitch"), "Pitch", 0.001f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::timesqueezeSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::TimesqueezeAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("timesqueeze", suffix)); };

        engine_.setTimePercent(v("time"));
        engine_.setPitchRatio(v("pitch"));
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::TimesqueezeAlgorithm engine_;
};
}
