#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/MultiShiftAlgorithm.h"
#include "dsp/schema/MultiShiftSchema.h"

#include <span>

// Adapts dsp::graphs::MultiShiftAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// MultiShiftPluginProcessor.cpp's own layout/processBlock, namespaced
// under "multiShift" (Algorithm 116). Two independent pitch-shift
// channels plus two independent dry delay taps, each pitch shifter's own
// input additionally patchable from any two of the four sources.
namespace loom::browser
{
namespace multishift_detail
{
using dsp::algorithms::MultiShift;

// Order matches dsp::algorithms::MultiShift::Source exactly.
inline const juce::StringArray kSourceNames = { "L Pitch", "R Pitch", "L Delay", "R Delay" };
}

class MultiShiftAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "multiShift"; }
    const char* displayName() const override { return "Eventide Multi-Shift"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace multishift_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("multiShift", suffix); };
        auto sourceParam = [&](const char* suffix, const juce::String& name, MultiShift::Source defaultSource) {
            return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ pid(suffix), 1 }, name,
                                                                  kSourceNames, static_cast<int>(defaultSource));
        };

        params.push_back(floatParam(pid("leftCents"), "L Coarse/Fine", -3600.0f, 3600.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("rightCents"), "R Coarse/Fine", -3600.0f, 3600.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("leftPitchDelay"), "L Pitch Delay", 0.0f, 0.7f, 0.02f, "s"));
        params.push_back(floatParam(pid("rightPitchDelay"), "R Pitch Delay", 0.0f, 0.7f, 0.02f, "s"));
        params.push_back(floatParam(pid("leftDelay"), "L Delay", 0.0f, 0.7f, 0.1f, "s"));
        params.push_back(floatParam(pid("rightDelay"), "R Delay", 0.0f, 0.7f, 0.1f, "s"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("feedbackScale"), "Feedback", 0.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("image"), "Image", -1.0f, 1.0f, 0.0f));
        params.push_back(floatParam(pid("lPitchLevel"), "L Pitch Level", -100.0f, 100.0f, 100.0f, "%"));
        params.push_back(floatParam(pid("rPitchLevel"), "R Pitch Level", -100.0f, 100.0f, 100.0f, "%"));
        params.push_back(floatParam(pid("lDelayLevel"), "L Delay Level", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("rDelayLevel"), "R Delay Level", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("lPitchPan"), "L Pitch Pan", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("rPitchPan"), "R Pitch Pan", -1.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("lDelayPan"), "L Delay Pan", -1.0f, 1.0f, -1.0f));
        params.push_back(floatParam(pid("rDelayPan"), "R Delay Pan", -1.0f, 1.0f, 1.0f));

        params.push_back(floatParam(pid("leftFb1Amount"), "L Feedback 1", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(sourceParam("leftFb1Source", "L Fb1 Source", MultiShift::Source::kLPitch));
        params.push_back(floatParam(pid("leftFb2Amount"), "L Feedback 2", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(sourceParam("leftFb2Source", "L Fb2 Source", MultiShift::Source::kLDelay));
        params.push_back(floatParam(pid("rightFb1Amount"), "R Feedback 1", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(sourceParam("rightFb1Source", "R Fb1 Source", MultiShift::Source::kRPitch));
        params.push_back(floatParam(pid("rightFb2Amount"), "R Feedback 2", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(sourceParam("rightFb2Source", "R Fb2 Source", MultiShift::Source::kRDelay));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("leftDirection"), 1 }, "L Reverse", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("rightDirection"), 1 }, "R Reverse", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("leftXfadeSlow"), 1 }, "L Xfade Slow", false));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ pid("rightXfadeSlow"), 1 }, "R Xfade Slow", false));
        params.push_back(floatParam(pid("leftSplice"), "L Splice", 0.001f, 0.7f, 0.15f, "s"));
        params.push_back(floatParam(pid("rightSplice"), "R Splice", 0.001f, 0.7f, 0.15f, "s"));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::multiShiftSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::MultiShiftAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using dsp::algorithms::MultiShift;
        auto v = [&](const char* suffix) { return paramValue(apvts, prefixedId("multiShift", suffix)); };

        engine_.setLeftCents(v("leftCents"));
        engine_.setRightCents(v("rightCents"));
        engine_.setLeftPitchDelaySeconds(v("leftPitchDelay"));
        engine_.setRightPitchDelaySeconds(v("rightPitchDelay"));
        engine_.setLeftDelaySeconds(v("leftDelay"));
        engine_.setRightDelaySeconds(v("rightDelay"));
        engine_.setMix(v("mix"));
        engine_.setFeedbackScale(v("feedbackScale"));
        engine_.setImage(v("image"));
        engine_.setLPitchLevel(v("lPitchLevel"));
        engine_.setRPitchLevel(v("rPitchLevel"));
        engine_.setLDelayLevel(v("lDelayLevel"));
        engine_.setRDelayLevel(v("rDelayLevel"));
        engine_.setLPitchPan(v("lPitchPan"));
        engine_.setRPitchPan(v("rPitchPan"));
        engine_.setLDelayPan(v("lDelayPan"));
        engine_.setRDelayPan(v("rDelayPan"));

        engine_.setLeftFeedback1(v("leftFb1Amount"), static_cast<MultiShift::Source>(static_cast<int>(v("leftFb1Source"))));
        engine_.setLeftFeedback2(v("leftFb2Amount"), static_cast<MultiShift::Source>(static_cast<int>(v("leftFb2Source"))));
        engine_.setRightFeedback1(v("rightFb1Amount"),
                                   static_cast<MultiShift::Source>(static_cast<int>(v("rightFb1Source"))));
        engine_.setRightFeedback2(v("rightFb2Amount"),
                                   static_cast<MultiShift::Source>(static_cast<int>(v("rightFb2Source"))));

        engine_.setLeftDirection(v("leftDirection") >= 0.5f);
        engine_.setRightDirection(v("rightDirection") >= 0.5f);
        engine_.setLeftXfadeSlow(v("leftXfadeSlow") >= 0.5f);
        engine_.setRightXfadeSlow(v("rightXfadeSlow") >= 0.5f);
        engine_.setLeftSpliceSeconds(v("leftSplice"));
        engine_.setRightSpliceSeconds(v("rightSplice"));

        engine_.process(left, right);
    }

  private:
    dsp::graphs::MultiShiftAlgorithm engine_;
};
}
