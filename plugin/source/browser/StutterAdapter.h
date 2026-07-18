#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/StutterAlgorithm.h"
#include "dsp/schema/StutterSchema.h"

#include <functional>
#include <span>

// Adapts dsp::graphs::StutterAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// StutterPluginProcessor.cpp's own layout/processBlock, namespaced under
// "stutter" (Algorithm 112). Exposes both sweep generators, Auto mode,
// and momentary trigger checkboxes for the eight actions the Patch's 2
// footswitch events can't all reach - same pollTrigger() pattern the
// plugin's own processBlock uses (fire once on true, then snap back to
// false so the next click fires again).
namespace loom::browser
{
namespace stutter_detail
{
using dsp::algorithms::Stutter;

inline const juce::StringArray kSweepTargetNames = { "None", "Left", "Right", "Both" };
inline const juce::StringArray kProgramNames = { "Total Random", "Random Sweep", "Random Pitch", "Just Stutter" };

inline Stutter::SweepTarget sweepTargetFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return Stutter::SweepTarget::kLeft;
        case 2:
            return Stutter::SweepTarget::kRight;
        case 3:
            return Stutter::SweepTarget::kBoth;
        case 0:
        default:
            return Stutter::SweepTarget::kNone;
    }
}

inline Stutter::Program programFromIndex(int index)
{
    switch (index)
    {
        case 1:
            return Stutter::Program::kRandomSweep;
        case 2:
            return Stutter::Program::kRandomPitch;
        case 3:
            return Stutter::Program::kJustStutter;
        case 0:
        default:
            return Stutter::Program::kTotalRandom;
    }
}

inline void pollTrigger(juce::AudioProcessorValueTreeState& apvts, const juce::String& id,
                         const std::function<void()>& action)
{
    auto* param = apvts.getParameter(id);
    if (param->getValue() >= 0.5f)
    {
        action();
        param->setValueNotifyingHost(0.0f);
    }
}
}

class StutterAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "stutter"; }
    const char* displayName() const override { return "Eventide Stutter"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace stutter_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("stutter", suffix); };
        auto triggerParam = [&](const char* suffix, const juce::String& name) {
            return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid(suffix), 1 }, name, false);
        };

        params.push_back(floatParam(pid("length1"), "Length 1", 0.0f, 0.5f, 0.1f, "s"));
        params.push_back(floatParam(pid("length2"), "Length 2", 0.0f, 0.5f, 0.05f, "s"));
        params.push_back(floatParam(pid("count1"), "Count 1", 0.0f, 16.0f, 4.0f));
        params.push_back(floatParam(pid("count2"), "Count 2", 0.0f, 16.0f, 8.0f));
        params.push_back(floatParam(pid("leftCents"), "Left Coarse/Fine", -4800.0f, 1200.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("rightCents"), "Right Coarse/Fine", -4800.0f, 1200.0f, 0.0f, "cents"));
        params.push_back(floatParam(pid("leftDelay"), "Left Delay", 0.0f, 0.5f, 0.0f, "s"));
        params.push_back(floatParam(pid("rightDelay"), "Right Delay", 0.0f, 0.5f, 0.0f, "s"));
        params.push_back(floatParam(pid("leftFeedback"), "Left Feedback", 0.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("rightFeedback"), "Right Feedback", 0.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("up1Rate"), "Up 1 Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("up1Max"), "Up 1 Max", 0.0f, 2400.0f, 1200.0f, "cents"));
        params.push_back(floatParam(pid("dn1Rate"), "Dn 1 Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("dn1Min"), "Dn 1 Min", -2400.0f, 0.0f, -1200.0f, "cents"));
        params.push_back(floatParam(pid("up2Rate"), "Up 2 Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("up2Max"), "Up 2 Max", 0.0f, 2400.0f, 1200.0f, "cents"));
        params.push_back(floatParam(pid("dn2Rate"), "Dn 2 Rate", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("dn2Min"), "Dn 2 Min", -2400.0f, 0.0f, -1200.0f, "cents"));
        params.push_back(floatParam(pid("rand1Max"), "Rand 1 Max", -1200.0f, 1200.0f, 1200.0f, "cents"));
        params.push_back(floatParam(pid("rand2Max"), "Rand 2 Max", -1200.0f, 1200.0f, 1200.0f, "cents"));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("sweepTarget1"), 1 }, "Sweep 1 Target", kSweepTargetNames, 1));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("sweepTarget2"), 1 }, "Sweep 2 Target", kSweepTargetNames, 2));
        params.push_back(floatParam(pid("leftMix"), "Left Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(floatParam(pid("rightMix"), "Right Mix", 0.0f, 1.0f, 1.0f));
        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("autoOn"), 1 }, "Auto", false));
        params.push_back(floatParam(pid("speed"), "Speed", 0.0f, 100.0f, 50.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("program"), 1 }, "Program", kProgramNames, 0));

        params.push_back(triggerParam("triggerStutter1", "Trigger Stutter 1"));
        params.push_back(triggerParam("triggerStutter2", "Trigger Stutter 2"));
        params.push_back(triggerParam("triggerSweepUp1", "Trigger Sweep Up 1"));
        params.push_back(triggerParam("triggerSweepDown1", "Trigger Sweep Down 1"));
        params.push_back(triggerParam("triggerRandomPitch1", "Trigger Random Pitch 1"));
        params.push_back(triggerParam("triggerSweepUp2", "Trigger Sweep Up 2"));
        params.push_back(triggerParam("triggerSweepDown2", "Trigger Sweep Down 2"));
        params.push_back(triggerParam("triggerRandomPitch2", "Trigger Random Pitch 2"));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::stutterSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::StutterAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace stutter_detail;
        auto pid = [](const char* suffix) { return prefixedId("stutter", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setLength1(v("length1"));
        engine_.setLength2(v("length2"));
        engine_.setCount1(static_cast<int>(v("count1") + 0.5f));
        engine_.setCount2(static_cast<int>(v("count2") + 0.5f));
        engine_.setLeftCoarseFineCents(v("leftCents"));
        engine_.setRightCoarseFineCents(v("rightCents"));
        engine_.setLeftDelaySeconds(v("leftDelay"));
        engine_.setRightDelaySeconds(v("rightDelay"));
        engine_.setLeftFeedback(v("leftFeedback"));
        engine_.setRightFeedback(v("rightFeedback"));
        engine_.setUp1(v("up1Rate"), v("up1Max"));
        engine_.setDn1(v("dn1Rate"), v("dn1Min"));
        engine_.setUp2(v("up2Rate"), v("up2Max"));
        engine_.setDn2(v("dn2Rate"), v("dn2Min"));
        engine_.setRand1Max(v("rand1Max"));
        engine_.setRand2Max(v("rand2Max"));
        engine_.setSweepTarget1(sweepTargetFromIndex(static_cast<int>(v("sweepTarget1"))));
        engine_.setSweepTarget2(sweepTargetFromIndex(static_cast<int>(v("sweepTarget2"))));
        engine_.setLeftMix(v("leftMix"));
        engine_.setRightMix(v("rightMix"));
        engine_.setAuto(v("autoOn") >= 0.5f);
        engine_.setSpeed(v("speed"));
        engine_.setProgram(programFromIndex(static_cast<int>(v("program"))));

        pollTrigger(apvts, pid("triggerStutter1"), [this] { engine_.triggerStutter1(); });
        pollTrigger(apvts, pid("triggerStutter2"), [this] { engine_.triggerStutter2(); });
        pollTrigger(apvts, pid("triggerSweepUp1"), [this] { engine_.triggerSweepUp1(); });
        pollTrigger(apvts, pid("triggerSweepDown1"), [this] { engine_.triggerSweepDown1(); });
        pollTrigger(apvts, pid("triggerRandomPitch1"), [this] { engine_.triggerRandomPitch1(); });
        pollTrigger(apvts, pid("triggerSweepUp2"), [this] { engine_.triggerSweepUp2(); });
        pollTrigger(apvts, pid("triggerSweepDown2"), [this] { engine_.triggerSweepDown2(); });
        pollTrigger(apvts, pid("triggerRandomPitch2"), [this] { engine_.triggerRandomPitch2(); });

        engine_.process(left, right);
    }

  private:
    dsp::graphs::StutterAlgorithm engine_;
};
}
