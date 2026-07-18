#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ChorusRvbAlgorithm.h"
#include "dsp/schema/ChorusRvbSchema.h"

#include <span>

// Adapts dsp::graphs::ChorusRvbAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// ChorusRvbPluginProcessor.cpp's own layout/processBlock, namespaced
// under "chorusRvb".
namespace loom::browser
{
class ChorusRvbAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "chorusRvb"; }
    const char* displayName() const override { return "Lexicon Chorus+Rvb"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("chorusRvb", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("chorusHighCut"), "Chorus High Cut", 1000.0f, 20000.0f, 10000.0f, "Hz", 0.4f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 2.2f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.3f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.6f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("attack"), "Attack", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("rvbOut"), "Rvb Out", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("preDelay"), "Pre Delay", 0.0f, 0.93f, 0.0f, "s"));
        params.push_back(floatParam(pid("earlyReflectionLevelLeft"), "Early Reflections L", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("earlyReflectionLevelRight"), "Early Reflections R", 0.0f, 1.0f, 0.2f));
        params.push_back(
          floatParam(pid("earlyReflectionDelayLeft"), "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(
          floatParam(pid("earlyReflectionDelayRight"), "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(floatParam(pid("ekoDelayLeft"), "Eko Delay L", 0.0f, 1.2f, 0.0f, "s"));
        params.push_back(floatParam(pid("ekoDelayRight"), "Eko Delay R", 0.0f, 1.2f, 0.0f, "s"));
        params.push_back(floatParam(pid("ekoFeedbackLeft"), "Eko Feedback L", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("ekoFeedbackRight"), "Eko Feedback R", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("spin"), "Spin", 0.0f, 1.0f, 0.5f));

        params.push_back(floatParam(pid("chorusMasterDepth"), "Chorus Master Depth", 0.0f, 200.0f, 100.0f, "%"));
        params.push_back(floatParam(pid("chorusMasterRate"), "Chorus Master Rate", 0.0f, 200.0f, 100.0f, "%"));

        static constexpr float kDefaultDelay[6] = { 0.02f, 0.035f, 0.05f, 0.025f, 0.04f, 0.055f };
        static constexpr float kDefaultPan[6] = { -0.7f, -0.4f, -0.15f, 0.15f, 0.4f, 0.7f };
        static constexpr float kDefaultDepth[6] = { 12.0f, 18.0f, 24.0f, 14.0f, 20.0f, 26.0f };
        static constexpr float kDefaultRate[6] = { 0.25f, 0.31f, 0.19f, 0.28f, 0.22f, 0.34f };
        for (int i = 0; i < 6; ++i)
        {
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.365f,
                                         kDefaultDelay[i], "s", 0.5f));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f, 0.5f));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.15f));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Depth"),
                                         "Voice " + juce::String(i + 1) + " Depth", 0.0f, 500.0f,
                                         kDefaultDepth[i], "ms"));
            params.push_back(floatParam(voiceParamId("chorusRvb", i, "Rate"),
                                         "Voice " + juce::String(i + 1) + " Rate", 0.0f, 3.5f,
                                         kDefaultRate[i], "Hz"));
        }

        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("freeze"), 1 }, "Freeze", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::chorusRvbSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ChorusRvbAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("chorusRvb", suffix)); };

        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));
        engine_.setInPan(v("inPanLeft"), v("inPanRight"));
        engine_.setChorusHighCut(v("chorusHighCut"));

        engine_.setDecaySeconds(v("decay"));
        engine_.setLowRatio(v("lowRatio"));
        engine_.setCrossoverFrequency(v("crossover"));
        engine_.setDamping(v("damping"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setSize(v("size"));
        engine_.setLink(v("link") >= 0.5f);
        engine_.setAttack(v("attack"));
        engine_.setRvbOut(v("rvbOut"));
        engine_.setPreDelaySeconds(v("preDelay"));
        engine_.setEarlyReflectionLevel(v("earlyReflectionLevelLeft"), v("earlyReflectionLevelRight"));
        engine_.setEarlyReflectionDelaySeconds(v("earlyReflectionDelayLeft"), v("earlyReflectionDelayRight"));
        engine_.setEkoDelaySeconds(v("ekoDelayLeft"), v("ekoDelayRight"));
        engine_.setEkoFeedback(v("ekoFeedbackLeft"), v("ekoFeedbackRight"));
        engine_.setSpin(v("spin"));

        engine_.setChorusMaster(v("chorusMasterDepth"), v("chorusMasterRate"));

        for (int i = 0; i < 6; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("chorusRvb", i, suffix));
            };
            engine_.setVoiceDelay(i, vv("Delay"));
            engine_.setVoiceLevel(i, vv("Level"));
            engine_.setVoicePan(i, vv("Pan"));
            engine_.setVoiceFeedback(i, vv("Feedback"));
            engine_.setVoiceChorus(i, vv("Depth"), vv("Rate"));
        }

        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

  private:
    dsp::graphs::ChorusRvbAlgorithm engine_;
};
}
