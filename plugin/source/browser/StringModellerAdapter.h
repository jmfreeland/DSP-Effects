#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/StringModellerAlgorithm.h"
#include "dsp/schema/StringModellerSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::StringModellerAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// StringModellerPluginProcessor.cpp's own layout/processBlock,
// namespaced under "stringModeller" (Algorithm 118). Six Karplus-Strong
// string resonators plus a Pluck trigger substituting for the missing
// MIDI note-on.
namespace loom::browser
{
class StringModellerAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "stringModeller"; }
    const char* displayName() const override { return "Eventide String Modeller"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("stringModeller", suffix); };

        params.push_back(floatParam(pid("pitch"), "Pitch", -100.0f, 100.0f, 0.0f, "st"));
        params.push_back(floatParam(pid("decay"), "Decay", 0.0f, 100.0f, 60.0f));
        params.push_back(floatParam(pid("gate"), "Gate", 1.0f, 100.0f, 30.0f));
        params.push_back(floatParam(pid("freq"), "Freq", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("qfac"), "Qfac", 0.0f, 100.0f, 30.0f));
        params.push_back(floatParam(pid("bright"), "Bright", 0.0f, 100.0f, 60.0f));
        params.push_back(floatParam(pid("highAmt"), "High Amt", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("bandAmt"), "Band Amt", -100.0f, 100.0f, 60.0f, "%"));
        params.push_back(floatParam(pid("lowAmt"), "Low Amt", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("inAmt"), "In Amt", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("chorus"), "Chorus", 0.0f, 100.0f, 40.0f));
        params.push_back(floatParam(pid("chorusSpeed"), "Speed", 0.0f, 100.0f, 30.0f));
        params.push_back(floatParam(pid("chorusDepth"), "Depth", 0.0f, 100.0f, 50.0f));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 100.0f, 60.0f, "%"));

        static constexpr std::array<float, 6> kDefaultNoteHz = { 82.41f, 110.00f, 146.83f, 196.00f, 246.94f, 329.63f };
        for (std::size_t i = 0; i < kDefaultNoteHz.size(); ++i)
        {
            auto n = juce::String(i + 1);
            params.push_back(
              floatParam(pid(("note" + n).toRawUTF8()), "Note " + n, 16.0f, 8000.0f, kDefaultNoteHz[i], "Hz"));
        }

        params.push_back(
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ pid("trigger"), 1 }, "Pluck", false));

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::stringModellerSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::StringModellerAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        auto pid = [](const char* suffix) { return prefixedId("stringModeller", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setPitch(v("pitch"));
        engine_.setDecay(v("decay"));
        engine_.setGateAmount(v("gate"));
        engine_.setFreq(v("freq"));
        engine_.setQfac(v("qfac"));
        engine_.setBright(v("bright"));
        engine_.setHighAmt(v("highAmt"));
        engine_.setBandAmt(v("bandAmt"));
        engine_.setLowAmt(v("lowAmt"));
        engine_.setInAmt(v("inAmt"));
        engine_.setChorus(v("chorus"));
        engine_.setChorusSpeed(v("chorusSpeed"));
        engine_.setChorusDepth(v("chorusDepth"));
        engine_.setMix(v("mix"));

        for (int i = 0; i < 6; ++i)
        {
            auto n = juce::String(i + 1);
            engine_.setNoteHz(i, paramValue(apvts, pid(("note" + n).toRawUTF8())));
        }

        auto* trigger = apvts.getParameter(pid("trigger"));
        if (trigger->getValue() >= 0.5f)
        {
            engine_.trigger();
            trigger->setValueNotifyingHost(0.0f);
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::StringModellerAlgorithm engine_;
};
}
