#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/BandDelayAlgorithm.h"
#include "dsp/schema/BandDelaySchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::BandDelayAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// BandDelayPluginProcessor.cpp's own layout/processBlock, namespaced
// under "bandDelay" (Algorithm 117). One shared DelayLine with 8
// independently-settable read taps, each feeding its own
// StateVariableFilter, output Level, and Pan.
namespace loom::browser
{
class BandDelayAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "bandDelay"; }
    const char* displayName() const override { return "Eventide Band Delay"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("bandDelay", suffix); };

        params.push_back(floatParam(pid("globalDelay"), "Global Delay", 0.0f, 100.0f, 100.0f, "%"));
        params.push_back(floatParam(pid("globalFrequency"), "Global Frequency", -128.0f, 128.0f, 0.0f, "st"));
        params.push_back(floatParam(pid("globalQ"), "Global Q", 0.0f, 100.0f, 100.0f, "%"));
        params.push_back(floatParam(pid("globalPan"), "Global Pan", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("feedbackDelay"), "Feedback Delay", 0.0f, 1.485f, 0.3f, "s"));
        params.push_back(floatParam(pid("feedback"), "Feedback", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));

        static constexpr std::array<float, 8> kDefaultBaseHz = { 110.0f, 220.0f, 330.0f, 440.0f,
                                                                   660.0f, 880.0f, 1320.0f, 2200.0f };
        static constexpr std::array<float, 8> kDefaultDelaysMs = { 83, 149, 227, 311, 401, 487, 571, 661 };
        static constexpr std::array<float, 8> kDefaultPans = { -1.0f, 1.0f, -0.6f, 0.6f, -0.3f, 0.3f, -0.1f, 0.1f };
        for (std::size_t i = 0; i < kDefaultBaseHz.size(); ++i)
        {
            auto n = juce::String(i + 1);
            params.push_back(
              floatParam(pid(("baseHz" + n).toRawUTF8()), "Base Hz " + n, 20.0f, 10000.0f, kDefaultBaseHz[i], "Hz"));
            params.push_back(floatParam(pid(("cents" + n).toRawUTF8()), "Cents " + n, 0.0f, 12800.0f, 0.0f, "ct"));
            params.push_back(floatParam(pid(("q" + n).toRawUTF8()), "Q " + n, 0.0f, 999.0f, 999.0f));
            params.push_back(
              floatParam(pid(("delay" + n).toRawUTF8()), "Delay " + n, 0.0f, 1496.0f, kDefaultDelaysMs[i], "ms"));
            params.push_back(floatParam(pid(("level" + n).toRawUTF8()), "Level " + n, -100.0f, 100.0f,
                                         (i % 2 == 0) ? 100.0f : -100.0f, "%"));
            params.push_back(floatParam(pid(("pan" + n).toRawUTF8()), "Pan " + n, -1.0f, 1.0f, kDefaultPans[i]));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::bandDelaySchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::BandDelayAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto pid = [](const char* suffix) { return prefixedId("bandDelay", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setGlobalDelay(v("globalDelay"));
        engine_.setGlobalFrequency(v("globalFrequency"));
        engine_.setGlobalQ(v("globalQ"));
        engine_.setGlobalPan(v("globalPan"));
        engine_.setFeedbackDelaySeconds(v("feedbackDelay"));
        engine_.setFeedback(v("feedback"));
        engine_.setMix(v("mix"));

        for (int i = 0; i < 8; ++i)
        {
            auto n = juce::String(i + 1);
            engine_.setFilterBaseHz(i, paramValue(apvts, pid(("baseHz" + n).toRawUTF8())));
            engine_.setFilterFrequencyCents(i, paramValue(apvts, pid(("cents" + n).toRawUTF8())));
            engine_.setFilterQ(i, paramValue(apvts, pid(("q" + n).toRawUTF8())));
            engine_.setFilterDelayMs(i, paramValue(apvts, pid(("delay" + n).toRawUTF8())));
            engine_.setFilterLevel(i, paramValue(apvts, pid(("level" + n).toRawUTF8())));
            engine_.setFilterPan(i, paramValue(apvts, pid(("pan" + n).toRawUTF8())));
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::BandDelayAlgorithm engine_;
};
}
