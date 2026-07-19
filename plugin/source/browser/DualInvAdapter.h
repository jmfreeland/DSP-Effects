#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/DualInvAlgorithm.h"
#include "dsp/schema/DualInvSchema.h"

#include <span>

// Adapts dsp::graphs::DualInvAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// DualInvPluginProcessor.cpp's own layout/processBlock, namespaced
// under "dualInv". Voice pitch defaults use the corrected +-12 cents -
// see DualChmbAdapter.h's note on the shared bug fix.
namespace loom::browser
{
namespace dualinv_detail
{
inline const juce::StringArray kSendsNames { "Stereo", "L=Rvb, R=FX", "Mono", "L=FX, R=Rvb" };
inline const juce::StringArray kReturnsNames { "Stereo", "Rvb=L, FX=R", "Mono", "FX=L, Rvb=R" };
inline const juce::StringArray kRoutingNames { "Parallel", "Rvb into FX", "FX into Rvb" };
}

class DualInvAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "dualInv"; }
    const char* displayName() const override { return "Lexicon Dual-Inv"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace dualinv_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("dualInv", suffix); };

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("sends"), 1 }, "Sends", kSendsNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("returns"), 1 }, "Returns", kReturnsNames, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("routing"), 1 }, "Routing", kRoutingNames, 0));
        params.push_back(floatParam(pid("rvbInLevel"), "Rvb In Lvl", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("fxInLevel"), "FX In Lvl", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("rvbMix"), "Rvb Mix", 0.0f, 1.0f, 0.8f));
        params.push_back(floatParam(pid("fxMix"), "FX Mix", 0.0f, 1.0f, 1.0f));

        params.push_back(floatParam(pid("duration"), "Duration", 0.05f, 10.0f, 2.5f, "s", 0.5f));
        params.push_back(floatParam(pid("lowSlope"), "Low Slope", -1.0f, 1.0f, -0.3f));
        params.push_back(floatParam(pid("midSlope"), "Mid Slope", -1.0f, 1.0f, -0.3f));
        params.push_back(floatParam(pid("crossover"), "Crossover", 100.0f, 2000.0f, 400.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("damping"), "Damping", 0.0f, 1.0f, 0.4f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("size"), "Size", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("shape"), "Shape", 0.0f, 1.0f, 0.3f));
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

        static constexpr float kDefaultDelay[2] = { 0.02f, 0.03f };
        static constexpr float kDefaultCents[2] = { 12.0f, -12.0f };
        static constexpr float kDefaultPan[2] = { -0.7f, 0.7f };
        for (int i = 0; i < 2; ++i)
        {
            params.push_back(floatParam(voiceParamId("dualInv", i, "Delay"),
                                         "Voice " + juce::String(i + 1) + " Delay", 0.0f, 1.25f,
                                         kDefaultDelay[i], "s", 0.5f));
            params.push_back(floatParam(voiceParamId("dualInv", i, "Cents"),
                                         "Voice " + juce::String(i + 1) + " Pitch", -3600.0f, 3600.0f,
                                         kDefaultCents[i], "cents"));
            params.push_back(floatParam(voiceParamId("dualInv", i, "Level"),
                                         "Voice " + juce::String(i + 1) + " Level", 0.0f, 1.0f, 0.7f));
            params.push_back(floatParam(voiceParamId("dualInv", i, "Pan"),
                                         "Voice " + juce::String(i + 1) + " Pan", -1.0f, 1.0f,
                                         kDefaultPan[i]));
            params.push_back(floatParam(voiceParamId("dualInv", i, "Feedback"),
                                         "Voice " + juce::String(i + 1) + " Fbk", -1.0f, 1.0f, 0.0f));
            params.push_back(floatParam(voiceParamId("dualInv", i, "CrossFeedback"),
                                         "Voice " + juce::String(i + 1) + " X-Fbk", -1.0f, 1.0f, 0.0f));
        }

        params.push_back(floatParam(pid("splice"), "Splice", 0.001f, 0.05f, 0.004f, "s"));
        params.push_back(floatParam(pid("fxWidth"), "FX Width", -360.0f, 360.0f, 45.0f, "deg"));
        params.push_back(floatParam(pid("hiCut"), "Hi Cut", 1000.0f, 20000.0f, 18000.0f, "Hz", 0.4f));
        params.push_back(floatParam(pid("fxAdjust"), "FX Adjust", -73.0f, 12.0f, 0.0f, "dB"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::dualInvSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::DualInvAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("dualInv", suffix)); };

        engine_.setSends(static_cast<dsp::Submixer::Sends>(static_cast<int>(v("sends"))));
        engine_.setReturns(static_cast<dsp::Submixer::Returns>(static_cast<int>(v("returns"))));
        engine_.setRouting(
          static_cast<dsp::graphs::DualInvAlgorithm::Routing>(static_cast<int>(v("routing"))));
        engine_.setRvbInLevel(v("rvbInLevel"));
        engine_.setFxInLevel(v("fxInLevel"));
        engine_.setRvbMix(v("rvbMix"));
        engine_.setFxMix(v("fxMix"));

        engine_.setDuration(v("duration"));
        engine_.setLowSlope(v("lowSlope"));
        engine_.setMidSlope(v("midSlope"));
        engine_.setCrossoverFrequency(v("crossover"));
        engine_.setDamping(v("damping"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setSize(v("size"));
        engine_.setShape(v("shape"));
        engine_.setRvbIn(v("rvbIn"));
        engine_.setRvbOut(v("rvbOut"));
        engine_.setPreDelaySeconds(v("preDelay"));
        engine_.setEarlyReflectionLevel(v("earlyReflectionLevelLeft"), v("earlyReflectionLevelRight"));
        engine_.setEarlyReflectionDelaySeconds(v("earlyReflectionDelayLeft"), v("earlyReflectionDelayRight"));
        engine_.setSpin(v("spin"));

        for (int i = 0; i < 2; ++i)
        {
            auto vv = [&](const char* suffix) {
                return paramValue(apvts, voiceParamId("dualInv", i, suffix));
            };
            engine_.setVoice(i, vv("Delay"), vv("Cents"), vv("Level"), vv("Pan"));
            engine_.setVoiceFeedback(i, vv("Feedback"), vv("CrossFeedback"));
        }
        engine_.setSpliceSeconds(v("splice"));

        engine_.setFxWidth(v("fxWidth"));
        engine_.setHiCut(v("hiCut"));
        engine_.setFxAdjustDb(v("fxAdjust"));
        engine_.setMix(v("mix"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::DualInvAlgorithm engine_;
};
}
