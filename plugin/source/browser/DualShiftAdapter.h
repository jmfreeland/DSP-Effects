#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/DualShiftAlgorithm.h"
#include "dsp/schema/DualShiftSchema.h"

#include <span>

// Adapts dsp::graphs::DualShiftAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// DualShiftPluginProcessor.cpp's own layout/processBlock, namespaced
// under "dualShift". Two fully independent per-channel voices with no
// shared input or feedback point (Algorithm 102); Left/Right Voice cents
// default to 0.0 (unison), not the Layered/Stereo Shift harmonizer
// interval, so there's no detune-bug question here.
namespace loom::browser
{
class DualShiftAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "dualShift"; }
    const char* displayName() const override { return "Eventide Dual Shift"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("dualShift", suffix); };

        params.push_back(floatParam(pid("grain"), "Grain", 0.01f, 0.3f, 0.07f, "s"));
        params.push_back(floatParam(pid("leftDelay"), "Left Delay", 0.0f, 0.5f, 0.05f, "s"));
        params.push_back(floatParam(pid("rightDelay"), "Right Delay", 0.0f, 0.5f, 0.05f, "s"));
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
        return dsp::schema::dualShiftSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::DualShiftAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("dualShift", suffix)); };

        engine_.setGrainSeconds(v("grain"));
        engine_.setLeftDelaySeconds(v("leftDelay"));
        engine_.setRightDelaySeconds(v("rightDelay"));
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
    dsp::graphs::DualShiftAlgorithm engine_;
};
}
