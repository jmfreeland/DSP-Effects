#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/StereoShiftAlgorithm.h"
#include "dsp/schema/StereoShiftSchema.h"

#include <span>

// Adapts dsp::graphs::StereoShiftAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// StereoShiftPluginProcessor.cpp's own layout/processBlock, namespaced
// under "stereoShift". Independent per-channel signal paths like Dual
// Shift, but one shared Delay/Cents/Feedback/Mix value drives both
// (Algorithm 103). The 700-cent Shift default is unchanged from the
// plugin's own - a deliberate harmonizer interval (a fifth) at 50% Mix,
// not the accidental-detune bug the Lexicon Dual/Quad family had (see
// docs/eventide-stereo-shift.md).
namespace loom::browser
{
class StereoShiftAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "stereoShift"; }
    const char* displayName() const override { return "Eventide Stereo Shift"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("stereoShift", suffix); };

        params.push_back(floatParam(pid("grain"), "Grain", 0.01f, 0.3f, 0.07f, "s"));
        params.push_back(floatParam(pid("delay"), "Delay", 0.0f, 0.5f, 0.05f, "s"));
        params.push_back(floatParam(pid("cents"), "Shift", -2400.0f, 1200.0f, 700.0f, "cents"));
        params.push_back(floatParam(pid("feedback"), "Feedback", 0.0f, 0.99f, 0.0f));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 0.5f));
        params.push_back(floatParam(pid("inLevelLeft"), "In Level L", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("inLevelRight"), "In Level R", -1.0f, 1.0f, 1.0f));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::stereoShiftSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::StereoShiftAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("stereoShift", suffix)); };

        engine_.setGrainSeconds(v("grain"));
        engine_.setDelaySeconds(v("delay"));
        engine_.setCents(v("cents"));
        engine_.setFeedback(v("feedback"));
        engine_.setMix(v("mix"));
        engine_.setInLevel(v("inLevelLeft"), v("inLevelRight"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::StereoShiftAlgorithm engine_;
};
}
