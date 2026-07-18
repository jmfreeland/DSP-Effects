#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/SweptReverbAlgorithm.h"
#include "dsp/schema/SweptReverbSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::SweptReverbAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// SweptReverbPluginProcessor.cpp's own layout/processBlock, namespaced
// under "sweptReverb" (Algorithm 106). Same six-swept-line shape as
// Swept Combs, feeding a Householder-mixed tank instead of a stereo
// mixer - one shared Feedback rather than per-line.
namespace loom::browser
{
namespace sweptreverb_detail
{
inline constexpr int kNumLines = dsp::algorithms::SweptReverb::kNumLines;

inline juce::String lineParamId(const char* prefix, const char* suffix, int line)
{
    return juce::String(prefix) + "_line" + juce::String(line) + suffix;
}
}

class SweptReverbAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "sweptReverb"; }
    const char* displayName() const override { return "Eventide Swept Reverb"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace sweptreverb_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("sweptReverb", suffix); };

        params.push_back(floatParam(pid("masterDelay"), "Master Delay", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("masterRate"), "Master Rate", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("masterDepth"), "Master Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("feedback"), "Feedback", -1.0f, 1.0f, 0.7f));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("repeat"), 1 }, "Repeat", false));

        static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 37, 53, 71, 97, 131, 179 };
        static constexpr std::array<float, kNumLines> kDefaultRates = { 25, 40, 55, 30, 45, 60 };
        for (int i = 0; i < kNumLines; ++i)
        {
            auto idx = static_cast<std::size_t>(i);
            params.push_back(floatParam(lineParamId("sweptReverb", "Delay", i),
                                         "Line " + juce::String(i + 1) + " Delay", 0.0f, 225.0f,
                                         kDefaultDelaysMs[idx], "ms"));
            params.push_back(floatParam(lineParamId("sweptReverb", "Rate", i),
                                         "Line " + juce::String(i + 1) + " Rate", 0.0f, 100.0f,
                                         kDefaultRates[idx]));
            params.push_back(floatParam(lineParamId("sweptReverb", "Depth", i),
                                         "Line " + juce::String(i + 1) + " Depth", 0.0f, 100.0f, 30.0f));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::sweptReverbSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::SweptReverbAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace sweptreverb_detail;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("sweptReverb", suffix)); };

        engine_.setMasterDelay(v("masterDelay"));
        engine_.setMasterRate(v("masterRate"));
        engine_.setMasterDepth(v("masterDepth"));
        engine_.setFeedback(v("feedback"));
        engine_.setMix(v("mix"));
        engine_.setRepeat(v("repeat") >= 0.5f);

        for (int i = 0; i < kNumLines; ++i)
        {
            engine_.setLineDelayMs(i, paramValue(apvts, lineParamId("sweptReverb", "Delay", i)));
            engine_.setLineRate(i, paramValue(apvts, lineParamId("sweptReverb", "Rate", i)));
            engine_.setLineDepth(i, paramValue(apvts, lineParamId("sweptReverb", "Depth", i)));
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::SweptReverbAlgorithm engine_;
};
}
