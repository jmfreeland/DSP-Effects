#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/SweptCombsAlgorithm.h"
#include "dsp/schema/SweptCombsSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::SweptCombsAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// SweptCombsPluginProcessor.cpp's own layout/processBlock, namespaced
// under "sweptCombs" (Algorithm 105). Six independently-settable
// ("Tedium") lines plus five Master ("Quickset") controls.
namespace loom::browser
{
namespace sweptcombs_detail
{
inline constexpr int kNumLines = dsp::algorithms::SweptCombs::kNumLines;

inline juce::String lineParamId(const char* prefix, const char* suffix, int line)
{
    return juce::String(prefix) + "_line" + juce::String(line) + suffix;
}
}

class SweptCombsAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "sweptCombs"; }
    const char* displayName() const override { return "Eventide Swept Combs"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace sweptcombs_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("sweptCombs", suffix); };

        params.push_back(floatParam(pid("masterDelay"), "Master Delay", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("masterRate"), "Master Rate", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("masterDepth"), "Master Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("masterFeedback"), "Master Feedback", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("width"), "Width", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("stereoInput"), 1 }, "Stereo Input", true));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("repeat"), 1 }, "Repeat", false));

        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 41, 67, 93, 127, 163, 211 };
        static constexpr std::array<float, kNumLines> kDefaultRates = { 20, 35, 50, 65, 80, 95 };
        static constexpr std::array<float, kNumLines> kDefaultPans = { -1.0f, -0.6f, -0.2f, 0.2f, 0.6f, 1.0f };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            params.push_back(floatParam(lineParamId("sweptCombs", "Delay", i),
                                         "Line " + juce::String(i + 1) + " Delay", 0.0f, 250.0f,
                                         kDefaultDelaysMs[idx], "ms"));
            params.push_back(floatParam(lineParamId("sweptCombs", "Rate", i),
                                         "Line " + juce::String(i + 1) + " Rate", 0.0f, 100.0f,
                                         kDefaultRates[idx]));
            params.push_back(floatParam(lineParamId("sweptCombs", "Depth", i),
                                         "Line " + juce::String(i + 1) + " Depth", 0.0f, 100.0f, 30.0f));
            params.push_back(floatParam(lineParamId("sweptCombs", "Feedback", i),
                                         "Line " + juce::String(i + 1) + " Feedback", -1.0f, 1.0f, 0.2f));
            params.push_back(floatParam(lineParamId("sweptCombs", "Pan", i),
                                         "Line " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPans[idx]));
            params.push_back(floatParam(lineParamId("sweptCombs", "Level", i),
                                         "Line " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.8f));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::sweptCombsSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::SweptCombsAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace sweptcombs_detail;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("sweptCombs", suffix)); };

        engine_.setMasterDelay(v("masterDelay"));
        engine_.setMasterRate(v("masterRate"));
        engine_.setMasterDepth(v("masterDepth"));
        engine_.setMasterFeedback(v("masterFeedback"));
        engine_.setWidth(v("width"));
        engine_.setMix(v("mix"));
        engine_.setStereoInput(v("stereoInput") >= 0.5f);
        engine_.setRepeat(v("repeat") >= 0.5f);

        for (int i = 0; i < kNumLines; ++i)
        {
            engine_.setLineDelayMs(i, paramValue(apvts, lineParamId("sweptCombs", "Delay", i)));
            engine_.setLineRate(i, paramValue(apvts, lineParamId("sweptCombs", "Rate", i)));
            engine_.setLineDepth(i, paramValue(apvts, lineParamId("sweptCombs", "Depth", i)));
            engine_.setLineFeedback(i, paramValue(apvts, lineParamId("sweptCombs", "Feedback", i)));
            engine_.setLinePan(i, paramValue(apvts, lineParamId("sweptCombs", "Pan", i)));
            engine_.setLineLevel(i, paramValue(apvts, lineParamId("sweptCombs", "Level", i)));
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::SweptCombsAlgorithm engine_;
};
}
