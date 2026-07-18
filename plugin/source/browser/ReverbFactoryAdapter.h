#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ReverbFactoryAlgorithm.h"
#include "dsp/schema/ReverbFactorySchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::ReverbFactoryAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// ReverbFactoryPluginProcessor.cpp's own layout/processBlock, namespaced
// under "reverbFactory" (Algorithm 107). Fixed (non-swept) lines with a
// dynamics Gate crossfading decay/tone between On/Off settings.
namespace loom::browser
{
namespace reverbfactory_detail
{
inline constexpr int kNumLines = dsp::algorithms::ReverbFactory::kNumLines;

inline juce::String lineParamId(const char* prefix, const char* suffix, int line)
{
    return juce::String(prefix) + "_line" + juce::String(line) + suffix;
}
}

class ReverbFactoryAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "reverbFactory"; }
    const char* displayName() const override { return "Eventide Reverb Factory"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace reverbfactory_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("reverbFactory", suffix); };

        params.push_back(floatParam(pid("predelay"), "Predelay", 0.0f, 0.5f, 0.02f, "s"));
        params.push_back(floatParam(pid("onDecay"), "On Decay", 0.1f, 10.0f, 2.5f, "s"));
        params.push_back(floatParam(pid("offDecay"), "Off Decay", 0.1f, 10.0f, 1.0f, "s"));
        params.push_back(floatParam(pid("gateTime"), "Gate Time", 0.0f, 25.0f, 1.0f, "s"));
        params.push_back(floatParam(pid("gateSpeed"), "Gate Speed", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("gateThreshold"), "Gate Threshold", 0.0f, 1.0f, 0.3f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("gateEnabled"), 1 }, "Gate Enabled", true));
        params.push_back(floatParam(pid("eqCrossover"), "EQ Crossover", 200.0f, 8000.0f, 2000.0f, "Hz"));
        params.push_back(floatParam(pid("onEqGain"), "On EQ Gain", -24.0f, 6.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("offEqGain"), "Off EQ Gain", -24.0f, 6.0f, -6.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 29, 43, 61, 79, 97, 113 };
        for (int i = 0; i < kNumLines; ++i)
        {
            params.push_back(floatParam(lineParamId("reverbFactory", "Delay", i),
                                         "Line " + juce::String(i + 1) + " Delay", 1.0f, 113.0f,
                                         kDefaultDelaysMs[static_cast<std::size_t>(i)], "ms"));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::reverbFactorySchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ReverbFactoryAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace reverbfactory_detail;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("reverbFactory", suffix)); };

        engine_.setPredelaySeconds(v("predelay"));
        engine_.setOnDecaySeconds(v("onDecay"));
        engine_.setOffDecaySeconds(v("offDecay"));
        engine_.setGateTimeSeconds(v("gateTime"));
        engine_.setGateSpeed(v("gateSpeed"));
        engine_.setGateThreshold(v("gateThreshold"));
        engine_.setGateEnabled(v("gateEnabled") >= 0.5f);
        engine_.setEqCrossoverHz(v("eqCrossover"));
        engine_.setOnEqGainDb(v("onEqGain"));
        engine_.setOffEqGainDb(v("offEqGain"));
        engine_.setMix(v("mix"));
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        for (int i = 0; i < kNumLines; ++i)
        {
            engine_.setLineDelayMs(i, paramValue(apvts, lineParamId("reverbFactory", "Delay", i)));
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::ReverbFactoryAlgorithm engine_;
};
}
