#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/DenseRoomAlgorithm.h"
#include "dsp/schema/DenseRoomSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::DenseRoomAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// DenseRoomPluginProcessor.cpp's own layout/processBlock, namespaced
// under "denseRoom" (Algorithm 114). A named evolution of Reverb
// Factory's 6-line Householder tank: adds a 3-stage diffusion chain and
// explicit per-line Pan/Level, drops the Gate for a single Rev Time.
namespace loom::browser
{
class DenseRoomAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "denseRoom"; }
    const char* displayName() const override { return "Eventide Dense Room"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("denseRoom", suffix); };

        params.push_back(floatParam(pid("predelay"), "Predelay", 0.0f, 0.5f, 0.02f, "s"));
        params.push_back(floatParam(pid("revTime"), "Rev Time", 0.1f, 10.0f, 2.0f, "s"));
        params.push_back(floatParam(pid("highCut"), "High Cut", 0.0f, 1.0f, 0.3f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.7f));
        params.push_back(floatParam(pid("position"), "Position", 0.0f, 1.0f, 0.3f));
        params.push_back(floatParam(pid("pan"), "Pan", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("earlyMix"), "Early Mix", 0.0f, 1.0f, 0.3f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.6f));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));

        static constexpr std::array<float, 3> kDefaultAllpassDelays = { 337.0f, 563.0f, 809.0f };
        for (std::size_t i = 0; i < kDefaultAllpassDelays.size(); ++i)
        {
            params.push_back(floatParam(pid((juce::String("allpassDelay") + juce::String(i + 1)).toRawUTF8()),
                                         juce::String("Allpass Delay ") + juce::String(i + 1), 0.0f, 5000.0f,
                                         kDefaultAllpassDelays[i], "smp"));
        }

        static constexpr std::array<float, 6> kDefaultLineDelaysMs = { 27, 41, 59, 73, 89, 109 };
        static constexpr std::array<float, 6> kDefaultLinePans = { -0.8f, 0.8f, -0.4f, 0.4f, -0.15f, 0.15f };
        for (std::size_t i = 0; i < kDefaultLineDelaysMs.size(); ++i)
        {
            params.push_back(floatParam(pid((juce::String("lineDelay") + juce::String(i + 1)).toRawUTF8()),
                                         juce::String("Delay ") + juce::String(i + 1), 1.0f, 113.0f,
                                         kDefaultLineDelaysMs[i], "ms"));
            params.push_back(floatParam(pid((juce::String("linePan") + juce::String(i + 1)).toRawUTF8()),
                                         juce::String("Pan ") + juce::String(i + 1), -1.0f, 1.0f,
                                         kDefaultLinePans[i]));
            params.push_back(floatParam(pid((juce::String("lineLevel") + juce::String(i + 1)).toRawUTF8()),
                                         juce::String("Level ") + juce::String(i + 1), -100.0f, 100.0f, 100.0f,
                                         "%"));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::denseRoomSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::DenseRoomAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto pid = [](const char* suffix) { return prefixedId("denseRoom", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setPredelaySeconds(v("predelay"));
        engine_.setRevTimeSeconds(v("revTime"));
        engine_.setHighCut(v("highCut"));
        engine_.setSize(v("size"));
        engine_.setPosition(v("position"));
        engine_.setPan(v("pan"));
        engine_.setEarlyMix(v("earlyMix"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setMix(v("mix"));

        for (int i = 0; i < 3; ++i)
        {
            engine_.setAllpassDelaySamples(
              i, paramValue(apvts, pid((juce::String("allpassDelay") + juce::String(i + 1)).toRawUTF8())));
        }
        for (int i = 0; i < 6; ++i)
        {
            engine_.setLineDelayMs(
              i, paramValue(apvts, pid((juce::String("lineDelay") + juce::String(i + 1)).toRawUTF8())));
            engine_.setLinePan(
              i, paramValue(apvts, pid((juce::String("linePan") + juce::String(i + 1)).toRawUTF8())));
            engine_.setLineLevel(
              i, paramValue(apvts, pid((juce::String("lineLevel") + juce::String(i + 1)).toRawUTF8())));
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::DenseRoomAlgorithm engine_;
};
}
