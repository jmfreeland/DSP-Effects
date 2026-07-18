#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/InfiniteAlgorithm.h"
#include "dsp/schema/ReverbCoreSchemas.h"

#include <span>

// Adapts dsp::graphs::InfiniteAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// InfinitePluginProcessor.cpp's own layout/processBlock, namespaced
// under "infinite".
namespace loom::browser
{
class InfiniteAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "infinite"; }
    const char* displayName() const override { return "Lexicon Infinite"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("infinite", suffix); };

        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inPanLeft"), "In Pan L", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("inPanRight"), "In Pan R", -1.0f, 1.0f, 1.0f));

        params.push_back(floatParam(pid("decay"), "Decay", 0.3f, 8.0f, 6.0f, "s", 0.5f));
        params.push_back(floatParam(pid("lowRatio"), "Low Ratio", 0.2f, 2.0f, 1.0f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("link"), 1 }, "Link", false));
        params.push_back(floatParam(pid("definition"), "Definition", 0.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("depth"), "Depth", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("rvbIn"), "Rvb In", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("rvbOut"), "Rvb Out", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("preDelay"), "Pre Delay", 0.0f, 0.93f, 0.01f, "s"));
        params.push_back(floatParam(pid("earlyReflectionLevelLeft"), "Early Reflections L", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("earlyReflectionLevelRight"), "Early Reflections R", 0.0f, 1.0f, 0.2f));
        params.push_back(
          floatParam(pid("earlyReflectionDelayLeft"), "Early Reflection Delay L", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(
          floatParam(pid("earlyReflectionDelayRight"), "Early Reflection Delay R", 0.0f, 1.2f, 0.03f, "s"));
        params.push_back(floatParam(pid("spin"), "Spin", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("chorus"), "Chorus", 0.0f, 1.0f, 0.3f));

        params.push_back(floatParam(pid("shape"), "Shape", 0.0f, 1.0f, 0.2f));
        params.push_back(floatParam(pid("spread"), "Spread", 0.0f, 1.0f, 0.6f));
        params.push_back(floatParam(pid("ekoDelayLeft"), "Eko Delay L", 0.0f, 1.2f, 0.06f, "s"));
        params.push_back(floatParam(pid("ekoDelayRight"), "Eko Delay R", 0.0f, 1.2f, 0.07f, "s"));
        params.push_back(floatParam(pid("ekoFeedbackLeft"), "Eko Feedback L", 0.0f, 0.95f, 0.2f));
        params.push_back(floatParam(pid("ekoFeedbackRight"), "Eko Feedback R", 0.0f, 0.95f, 0.2f));

        params.push_back(floatParam(pid("voiceDiffusion"), "Voice Diffusion", 0.0f, 1.0f, 0.0f));
        static constexpr float kDefaultDelay[4] = { 0.09f, 0.13f, 0.0f, 0.0f };
        static constexpr float kDefaultFeedback[4] = { 0.15f, 0.10f, 0.0f, 0.0f };
        static constexpr float kDefaultLevel[4] = { 0.25f, 0.18f, 0.0f, 0.0f };
        static constexpr float kDefaultPan[4] = { -0.3f, 0.3f, 0.0f, 0.0f };
        for (int i = 0; i < 4; ++i)
        {
            params.push_back(floatParam(voiceParamId("infinite", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.365f,
                                         kDefaultDelay[i], "s"));
            params.push_back(floatParam(voiceParamId("infinite", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Feedback", -1.0f, 1.0f,
                                         kDefaultFeedback[i]));
            params.push_back(floatParam(voiceParamId("infinite", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", -1.0f, 1.0f,
                                         kDefaultLevel[i]));
            params.push_back(floatParam(voiceParamId("infinite", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
        }
        params.push_back(floatParam(pid("voiceGlideResponse"), "Voice Glide Response", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("voiceGlideRange"), "Voice Glide Range", 0.0f, 1.365f, 0.0f, "s"));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("clear"), 1 }, "Clear", false));

        params.push_back(floatParam(pid("postDelayLeft"), "Post Delay L", 0.0f, 1.365f, 0.25f, "s"));
        params.push_back(floatParam(pid("postDelayRight"), "Post Delay R", 0.0f, 1.365f, 0.25f, "s"));
        params.push_back(
          floatParam(pid("postDelayGlideResponse"), "Post Delay Glide Response", 0.0f, 100.0f, 50.0f));
        params.push_back(
          floatParam(pid("postDelayGlideRange"), "Post Delay Glide Range", 0.0f, 1.365f, 0.0f, "s"));
        params.push_back(floatParam(pid("postDelayMix"), "Post Delay Mix", 0.0f, 1.0f, 0.15f));
        params.push_back(floatParam(pid("rvbWidth"), "Rvb Width", -360.0f, 360.0f, 0.0f, "deg"));
        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 0.75f));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 0.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.4f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("freeze"), 1 }, "Freeze", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::infiniteAlgorithmSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::InfiniteAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("infinite", suffix)); };

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
        engine_.setShape(v("shape"));
        engine_.setSpread(v("spread"));
        engine_.setEkoDelaySeconds(v("ekoDelayLeft"), v("ekoDelayRight"));
        engine_.setEkoFeedback(v("ekoFeedbackLeft"), v("ekoFeedbackRight"));
        engine_.setVoiceDiffusion(v("voiceDiffusion"));

        engine_.setVoiceGlide(v("voiceGlideResponse"), v("voiceGlideRange"));
        for (int i = 0; i < 4; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("infinite", i, suffix));
            };
            engine_.setVoice(i, vv("Delay"), vv("Feedback"), vv("Level"), vv("Pan"));
        }
        engine_.setClear(v("clear") >= 0.5f);

        engine_.setPostDelayGlide(v("postDelayGlideResponse"), v("postDelayGlideRange"));
        engine_.setPostDelaySeconds(v("postDelayLeft"), v("postDelayRight"));
        engine_.setPostDelayMix(v("postDelayMix"));
        engine_.setRvbWidth(v("rvbWidth"));
        engine_.setFxMix(v("fxMix"));
        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));
        engine_.setFrozen(v("freeze") >= 0.5f);

        engine_.process(left, right);
    }

  private:
    dsp::graphs::InfiniteAlgorithm engine_;
};
}
