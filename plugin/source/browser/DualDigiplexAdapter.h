#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/DualDigiplexAlgorithm.h"
#include "dsp/schema/DualDigiplexSchema.h"

#include <span>

// Adapts dsp::graphs::DualDigiplexAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// DualDigiplexPluginProcessor.cpp's own layout/processBlock, namespaced
// under "dualDigiplex" (Algorithm 110). Two hand-rolled Delay+Feedback+
// Glide channels sharing one Repeat flag and a Stereo/Mono input flag.
namespace loom::browser
{
class DualDigiplexAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "dualDigiplex"; }
    const char* displayName() const override { return "Eventide Dual Digiplex"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("dualDigiplex", suffix); };

        params.push_back(floatParam(pid("leftDelay"), "Left Delay", 0.0f, 0.7f, 0.2f, "s"));
        params.push_back(floatParam(pid("rightDelay"), "Right Delay", 0.0f, 0.7f, 0.3f, "s"));
        params.push_back(floatParam(pid("leftFeedback"), "Left Feedback", -1.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("rightFeedback"), "Right Feedback", -1.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("glideResponse"), "Glide Speed", 0.0f, 100.0f, 50.0f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("glideEnabled"), 1 }, "Glide Enabled", true));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("repeat"), 1 }, "Repeat", false));
        params.push_back(floatParam(pid("leftMix"), "Left Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("rightMix"), "Right Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("stereoInput"), 1 }, "Stereo Input", true));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::dualDigiplexSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::DualDigiplexAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("dualDigiplex", suffix)); };

        engine_.setGlide(v("glideResponse"), v("glideEnabled") >= 0.5f);
        engine_.setLeftDelaySeconds(v("leftDelay"));
        engine_.setRightDelaySeconds(v("rightDelay"));
        engine_.setLeftFeedback(v("leftFeedback"));
        engine_.setRightFeedback(v("rightFeedback"));
        engine_.setRepeat(v("repeat") >= 0.5f);
        engine_.setLeftMix(v("leftMix"));
        engine_.setRightMix(v("rightMix"));
        engine_.setStereoInput(v("stereoInput") >= 0.5f);

        engine_.process(left, right);
    }

  private:
    dsp::graphs::DualDigiplexAlgorithm engine_;
};
}
