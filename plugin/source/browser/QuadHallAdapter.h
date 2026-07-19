#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/QuadHallAlgorithm.h"
#include "dsp/schema/QuadHallSchema.h"

#include <span>

// Adapts dsp::graphs::QuadHallAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// QuadHallPluginProcessor.cpp's own layout/processBlock, namespaced
// under "quadHall".
namespace loom::browser
{
class QuadHallAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "quadHall"; }
    const char* displayName() const override { return "Lexicon Quad>Hall"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("quadHall", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.5f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.6f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.5f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("definition"), "Definition", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("depth"), "Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("rvbIn"), "Rvb In", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("rvbOut"), "Rvb Out", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("preDelay"), "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
        params.push_back(floatParam(pid("earlyReflectionLevelLeft"), "Early Reflections L", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("earlyReflectionLevelRight"), "Early Reflections R", 0.0f, 1.0f, 0.2f));
        params.push_back(
          floatParam(pid("earlyReflectionDelayLeft"), "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(
          floatParam(pid("earlyReflectionDelayRight"), "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(floatParam(pid("spin"), "Spin", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("chorus"), "Chorus", 0.0f, 1.0f, 0.3f));

        static constexpr float kDefaultDelay[4] = { 0.02f, 0.03f, 0.02f, 0.03f };
        static constexpr float kDefaultCents[4] = { 12.0f, -12.0f, 15.0f, -15.0f };
        static constexpr float kDefaultPan[4] = { -0.6f, -0.3f, 0.3f, 0.6f };
        for (int i = 0; i < 4; ++i)
        {
            params.push_back(floatParam(voiceParamId("quadHall", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.25f,
                                         kDefaultDelay[i], "s", 0.5f));
            params.push_back(floatParam(voiceParamId("quadHall", i, "Cents"),
                                         "Voice " + juce::String(i + 1) + " Pitch", -3600.0f, 3600.0f,
                                         kDefaultCents[i], "cents"));
            params.push_back(floatParam(voiceParamId("quadHall", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.5f));
            params.push_back(floatParam(voiceParamId("quadHall", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("quadHall", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.0f));
            params.push_back(floatParam(voiceParamId("quadHall", i, "CrossFeedback"),
                                         "Voice " + juce::String(i + 1) + " X-Fbk", -1.0f, 1.0f, 0.0f));
        }

        params.push_back(floatParam(pid("splice"), "Splice", 0.001f, 0.05f, 0.004f, "s"));
        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.35f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("freeze"), 1 }, "Freeze", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::quadHallSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::QuadHallAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("quadHall", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setInPan(v("inPanLeft"), v("inPanRight"));

        engine_.setDecaySeconds(v("decay"));
        engine_.setLowRatio(v("lowRatio"));
        engine_.setCrossoverFrequency(v("crossover"));
        engine_.setDamping(v("damping"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setSize(v("size"));
        engine_.setLink(v("link") >= 0.5f);
        engine_.setDefinition(v("definition"));
        engine_.setDepth(v("depth"));
        engine_.setRvbIn(v("rvbIn"));
        engine_.setRvbOut(v("rvbOut"));
        engine_.setPreDelaySeconds(v("preDelay"));
        engine_.setEarlyReflectionLevel(v("earlyReflectionLevelLeft"), v("earlyReflectionLevelRight"));
        engine_.setEarlyReflectionDelaySeconds(v("earlyReflectionDelayLeft"), v("earlyReflectionDelayRight"));
        engine_.setSpin(v("spin"));
        engine_.setChorus(v("chorus"));

        for (int i = 0; i < 4; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("quadHall", i, suffix));
            };
            engine_.setVoice(i, vv("Delay"), vv("Cents"), vv("Level"), vv("Pan"));
            engine_.setVoiceFeedback(i, vv("Feedback"), vv("CrossFeedback"));
        }
        engine_.setSpliceSeconds(v("splice"));

        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

  private:
    dsp::graphs::QuadHallAlgorithm engine_;
};
}
