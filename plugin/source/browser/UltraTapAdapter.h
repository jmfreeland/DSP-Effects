#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/UltraTapAlgorithm.h"
#include "dsp/schema/UltraTapSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::UltraTapAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// UltraTapPluginProcessor.cpp's own layout/processBlock, namespaced
// under "ultraTap" (Algorithm 108). The Quickset Spacing/Weights/Pans
// shape generators are one-shot presets applied on change (matching the
// manual's own "presets Tedium" behavior), so this adapter tracks the
// last-applied shape index itself, same as the plugin's own
// lastSpacingShape_/lastWeightsShape_/lastPansShape_ members.
namespace loom::browser
{
namespace ultratap_detail
{
inline constexpr int kNumTaps = dsp::algorithms::UltraTap::kNumTaps;
inline constexpr int kNumAllpassStages = dsp::algorithms::UltraTap::kNumAllpassStages;

inline const juce::StringArray kShapeNames = { "Constant",          "Linear Increasing", "Linear Decreasing",
                                                "Exponential Increasing", "Exponential Decreasing", "Random" };
inline const juce::StringArray kPanShapeNames = { "Center",       "Left",          "Right",     "Sweep L to R",
                                                    "Sweep R to L", "Spread from Center", "Merge to Center",
                                                    "Alternating",  "Random" };

inline juce::String tapParamId(const char* prefix, const char* suffix, int tap)
{
    return juce::String(prefix) + "_tap" + juce::String(tap) + suffix;
}

inline juce::String allpassParamId(const char* prefix, int stage)
{
    return juce::String(prefix) + "_allpass" + juce::String(stage) + "Delay";
}
}

class UltraTapAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "ultraTap"; }
    const char* displayName() const override { return "Eventide Ultra-Tap"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace ultratap_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("ultraTap", suffix); };

        params.push_back(floatParam(pid("length"), "Length", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("diffusion"), "Diffusion", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("width"), "Width", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("feedback"), "Feedback", -1.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("fbTap"), "Fb Tap", 1.0f, 12.0f, 12.0f));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("stereoInput"), 1 }, "Stereo Input", true));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("spacingShape"), 1 }, "Spacing Shape", kShapeNames, 1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("weightsShape"), 1 }, "Weights Shape", kShapeNames, 2));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("pansShape"), 1 }, "Pans Shape", kPanShapeNames, 5));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        static constexpr std::array<float, kNumAllpassStages> kDefaultAllpassMs = { 20.0f, 15.0f, 11.0f, 7.0f };
        for (int i = 0; i < kNumAllpassStages; ++i)
        {
            params.push_back(floatParam(allpassParamId("ultraTap", i), "Allpass " + juce::String(i + 1) + " Delay",
                                         0.0f, 800.0f, kDefaultAllpassMs[static_cast<std::size_t>(i)], "ms"));
        }

        for (int i = 0; i < kNumTaps; ++i)
        {
            params.push_back(floatParam(tapParamId("ultraTap", "Delay", i), "Tap " + juce::String(i + 1) + " Delay",
                                         0.0f, 1450.0f / kNumTaps * 2.0f, 1400.0f / kNumTaps, "ms"));
            params.push_back(floatParam(tapParamId("ultraTap", "Level", i), "Tap " + juce::String(i + 1) + " Level",
                                         0.0f, 1.0f, 0.7f));
            params.push_back(floatParam(tapParamId("ultraTap", "Pan", i), "Tap " + juce::String(i + 1) + " Pan",
                                         -1.0f, 1.0f, 0.0f));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::ultraTapSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::UltraTapAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
        lastSpacingShape_ = lastWeightsShape_ = lastPansShape_ = -1;
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace ultratap_detail;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("ultraTap", suffix)); };

        engine_.setLength(v("length"));
        engine_.setDiffusion(v("diffusion"));
        engine_.setWidth(v("width"));
        engine_.setFeedback(v("feedback"));
        engine_.setFbTap(static_cast<int>(v("fbTap") + 0.5f));
        engine_.setMix(v("mix"));
        engine_.setStereoInput(v("stereoInput") >= 0.5f);
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        for (int i = 0; i < kNumAllpassStages; ++i)
        {
            engine_.setAllpassDelayMs(i, paramValue(apvts, allpassParamId("ultraTap", i)));
        }
        for (int i = 0; i < kNumTaps; ++i)
        {
            engine_.setTapDelayMs(i, paramValue(apvts, tapParamId("ultraTap", "Delay", i)));
            engine_.setTapLevel(i, paramValue(apvts, tapParamId("ultraTap", "Level", i)));
            engine_.setTapPan(i, paramValue(apvts, tapParamId("ultraTap", "Pan", i)));
        }

        auto spacingShape = static_cast<int>(v("spacingShape"));
        if (spacingShape != lastSpacingShape_)
        {
            engine_.applySpacingShape(static_cast<dsp::algorithms::UltraTap::Shape>(spacingShape));
            lastSpacingShape_ = spacingShape;
        }
        auto weightsShape = static_cast<int>(v("weightsShape"));
        if (weightsShape != lastWeightsShape_)
        {
            engine_.applyWeightsShape(static_cast<dsp::algorithms::UltraTap::Shape>(weightsShape));
            lastWeightsShape_ = weightsShape;
        }
        auto pansShape = static_cast<int>(v("pansShape"));
        if (pansShape != lastPansShape_)
        {
            engine_.applyPansShape(static_cast<dsp::algorithms::UltraTap::PanShape>(pansShape));
            lastPansShape_ = pansShape;
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::UltraTapAlgorithm engine_;
    int lastSpacingShape_ = -1;
    int lastWeightsShape_ = -1;
    int lastPansShape_ = -1;
};
}
