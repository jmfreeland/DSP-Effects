#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ReverseShiftAlgorithm.h"
#include "dsp/schema/ReverseShiftSchema.h"

#include <span>

// Adapts dsp::graphs::ReverseShiftAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// ReverseShiftPluginProcessor.cpp's own layout/processBlock, namespaced
// under "reverseShift" (Algorithm 104). Left/Right Voice cents default
// to 0.0 (unison splice), not a harmonizer interval.
namespace loom::browser
{
class ReverseShiftAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "reverseShift"; }
    const char* displayName() const override { return "Eventide Reverse Shift"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("reverseShift", suffix); };

        params.push_back(floatParam(pid("grain"), "Grain", 0.01f, 0.3f, 0.07f, "s"));
        params.push_back(floatParam(pid("leftLength"), "Left Length", 0.02f, 1.4f, 0.15f, "s"));
        params.push_back(floatParam(pid("rightLength"), "Right Length", 0.02f, 1.4f, 0.15f, "s"));
        params.push_back(floatParam(pid("leftCents"), "Left Voice", -2400.0f, 1200.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("rightCents"), "Right Voice", -2400.0f, 1200.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("leftFeedback"), "Left Feedback", 0.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("rightFeedback"), "Right Feedback", 0.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("leftMix"), "Left Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("rightMix"), "Right Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::reverseShiftSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ReverseShiftAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("reverseShift", suffix)); };

        engine_.setGrainSeconds(v("grain"));
        engine_.setLeftLengthSeconds(v("leftLength"));
        engine_.setRightLengthSeconds(v("rightLength"));
        engine_.setLeftCents(v("leftCents"));
        engine_.setRightCents(v("rightCents"));
        engine_.setLeftFeedback(v("leftFeedback"));
        engine_.setRightFeedback(v("rightFeedback"));
        engine_.setLeftMix(v("leftMix"));
        engine_.setRightMix(v("rightMix"));
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::ReverseShiftAlgorithm engine_;
};
}
