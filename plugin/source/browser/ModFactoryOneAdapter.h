#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ModFactoryOneAlgorithm.h"
#include "dsp/schema/ModFactoryOneSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::ModFactoryOneAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// ModFactoryOnePluginProcessor.cpp's own layout/processBlock,
// namespaced under "modFactoryOne" (Algorithm 122). A 28-destination x
// 26-source patch matrix exposed as dropdowns, same technique as Patch
// Factory's own but at a larger scale.
namespace loom::browser
{
namespace modfactoryone_detail
{
using dsp::algorithms::ModFactoryOne;
using Destination = ModFactoryOne::Destination;
using Source = ModFactoryOne::Source;

// Order matches dsp::algorithms::ModFactoryOne::Source exactly, so the
// AudioParameterChoice index can be cast straight to the enum.
inline const juce::StringArray kSourceNames = {
    "Zero",      "Left Input", "Right Input", "Mixer 1",  "Mixer 2",     "Mixer 3",    "Mixer 4",
    "Amp Mod 1", "Amp Mod 2",  "Delay 1",     "Delay 2",  "Filter 1",    "Filter 2",   "Ducker 1",
    "Ducker 2",  "Envelope 1", "Envelope 2",  "LFO 1",    "LFO 2",       "Knob 1",     "Knob 2",
    "Noise Gen", "Fullscale",  "-Fullscale",  "ModScale 1", "ModScale 2"
};

inline const juce::StringArray kLfoWaveformNames = { "Sine",
                                                       "Square",
                                                       "Sawtooth",
                                                       "Triangle",
                                                       "Exp Sawtooth",
                                                       "Exp Triangle",
                                                       "Triggered Sine",
                                                       "Triggered Saw",
                                                       "Triggered Triangle",
                                                       "Triggered Exp Saw",
                                                       "Triggered Exp Triangle",
                                                       "Toggle Linear",
                                                       "Toggle Exponential" };
inline const juce::StringArray kFilterTypeNames = { "Lowpass", "Bandpass", "Highpass" };

struct DestinationInfo
{
    Destination destination;
    const char* suffix;
    const char* name;
    Source defaultSource;
};

// clang-format off
inline const std::array<DestinationInfo, static_cast<std::size_t>(Destination::kCount)> kDestinations = { {
    { Destination::kLeftOut,   "dstLeftOut",   "Left Out",    Source::kMixer1 },
    { Destination::kRightOut,  "dstRightOut",  "Right Out",   Source::kMixer1 },
    { Destination::kMix1aIn,   "dstMix1aIn",   "Mix1a In",    Source::kLeftInput },
    { Destination::kMix1bIn,   "dstMix1bIn",   "Mix1b In",    Source::kDelay1 },
    { Destination::kMix2aIn,   "dstMix2aIn",   "Mix2a In",    Source::kZero },
    { Destination::kMix2bIn,   "dstMix2bIn",   "Mix2b In",    Source::kZero },
    { Destination::kMix3aIn,   "dstMix3aIn",   "Mix3a In",    Source::kZero },
    { Destination::kMix3bIn,   "dstMix3bIn",   "Mix3b In",    Source::kZero },
    { Destination::kMix4aIn,   "dstMix4aIn",   "Mix4a In",    Source::kZero },
    { Destination::kMix4bIn,   "dstMix4bIn",   "Mix4b In",    Source::kZero },
    { Destination::kAm1In,     "dstAm1In",     "AM1 In",      Source::kZero },
    { Destination::kAm1Mod,    "dstAm1Mod",    "AM1 Mod",     Source::kZero },
    { Destination::kAm2In,     "dstAm2In",     "AM2 In",      Source::kZero },
    { Destination::kAm2Mod,    "dstAm2Mod",    "AM2 Mod",     Source::kZero },
    { Destination::kDly1In,    "dstDly1In",    "Dly1 In",     Source::kLeftInput },
    { Destination::kDly1Mod,   "dstDly1Mod",   "Dly1 Mod",    Source::kLfo1 },
    { Destination::kDly2In,    "dstDly2In",    "Dly2 In",     Source::kZero },
    { Destination::kDly2Mod,   "dstDly2Mod",   "Dly2 Mod",    Source::kZero },
    { Destination::kFilt1In,   "dstFilt1In",   "Filt1 In",    Source::kZero },
    { Destination::kFilt1Mod,  "dstFilt1Mod",  "Filt1 Mod",   Source::kZero },
    { Destination::kFilt2In,   "dstFilt2In",   "Filt2 In",    Source::kZero },
    { Destination::kFilt2Mod,  "dstFilt2Mod",  "Filt2 Mod",   Source::kZero },
    { Destination::kEnv1In,    "dstEnv1In",    "Env1 In",     Source::kZero },
    { Destination::kEnv2In,    "dstEnv2In",    "Env2 In",     Source::kZero },
    { Destination::kLfo1In,    "dstLfo1In",    "LFO1 In",     Source::kZero },
    { Destination::kLfo2In,    "dstLfo2In",    "LFO2 In",     Source::kZero },
    { Destination::kMdScl1In,  "dstMdScl1In",  "ModScale1 In",Source::kZero },
    { Destination::kMdScl2In,  "dstMdScl2In",  "ModScale2 In",Source::kZero },
} };
// clang-format on
}

class ModFactoryOneAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "modFactoryOne"; }
    const char* displayName() const override { return "Eventide Mod Factory One"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace modfactoryone_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("modFactoryOne", suffix); };

        params.push_back(floatParam(pid("bpm"), "BPM", 30.0f, 200.0f, 120.0f));
        params.push_back(floatParam(pid("knob1"), "Knob 1", 0.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("knob2"), "Knob 2", 0.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 100.0f, 100.0f, "%"));

        for (int i = 1; i <= 2; ++i)
        {
            auto n = juce::String(i);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };

            params.push_back(floatParam(chId("filterCutoff"), "Filter " + n + " Cutoff", 0.0f, 7000.0f, 1000.0f, "Hz"));
            params.push_back(floatParam(chId("filterQ"), "Filter " + n + " Q", 1.0f, 1000.0f, 1.0f));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
              juce::ParameterID{ chId("filterType"), 1 }, "Filter " + n + " Type", kFilterTypeNames, 0));
            params.push_back(
              floatParam(chId("filterMod"), "Filter " + n + " Mod Amount", 0.0f, 7000.0f, 0.0f, "Hz"));

            params.push_back(floatParam(chId("delayMs"), "Delay " + n, 0.0f, 700.0f, 300.0f, "ms"));
            params.push_back(floatParam(chId("delayBpm"), "Delay " + n + " BPM", 0.0f, 96.0f, 0.0f, "/24"));
            params.push_back(
              floatParam(chId("delayFeedback"), "Delay " + n + " Feedback", -100.0f, 100.0f, 0.0f, "%"));
            params.push_back(std::make_unique<juce::AudioParameterBool>(
              juce::ParameterID{ chId("delayLoop"), 1 }, "Delay " + n + " Loop", false));
            params.push_back(
              floatParam(chId("delayMod"), "Delay " + n + " Mod Amount", -500.0f, 500.0f, 0.0f, "ms"));

            params.push_back(floatParam(chId("lfoFreq"), "LFO " + n + " Freq", 0.0f, 300.0f, 1.0f, "Hz"));
            params.push_back(floatParam(chId("lfoBpm"), "LFO " + n + " BPM", 0.0f, 96.0f, 0.0f, "/24"));
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
              juce::ParameterID{ chId("lfoWaveform"), 1 }, "LFO " + n + " Waveform", kLfoWaveformNames, 0));
            params.push_back(
              floatParam(chId("lfoThreshold"), "LFO " + n + " Threshold", -40.0f, 0.0f, -20.0f, "dB"));
            params.push_back(floatParam(chId("lfoMod"), "LFO " + n + " Mod Amount", 0.0f, 300.0f, 0.0f, "Hz"));

            params.push_back(floatParam(chId("envAttack"), "Env " + n + " Attack", 0.0f, 1000.0f, 5.0f, "ms"));
            params.push_back(floatParam(chId("envDecay"), "Env " + n + " Decay", 0.0f, 1000.0f, 100.0f, "ms"));
            params.push_back(
              floatParam(chId("envThreshold"), "Env " + n + " Threshold", -40.0f, 0.0f, -20.0f, "dB"));
            params.push_back(floatParam(chId("envRatio"), "Env " + n + " Ratio", 1.0f, 100.0f, 4.0f, ":1"));

            params.push_back(
              floatParam(chId("ampModAmount"), "AmpMod " + n + " Amount", -200.0f, 200.0f, 100.0f, "%"));
            params.push_back(floatParam(chId("ampModOffset"), "AmpMod " + n + " Offset", -200.0f, 200.0f, 0.0f, "%"));

            params.push_back(
              floatParam(chId("modScaleAmount"), "ModScale " + n + " Amount", -100.0f, 100.0f, 100.0f, "%"));
        }

        for (int i = 1; i <= 4; ++i)
        {
            auto n = juce::String(i);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };
            params.push_back(floatParam(chId("mixAAmount"), "Mixer " + n + " A Amount", -100.0f, 100.0f,
                                         i == 1 ? 100.0f : 0.0f, "%"));
            params.push_back(floatParam(chId("mixBAmount"), "Mixer " + n + " B Amount", -100.0f, 100.0f,
                                         i == 1 ? 100.0f : 0.0f, "%"));
        }

        for (const auto& d : kDestinations)
        {
            params.push_back(std::make_unique<juce::AudioParameterChoice>(
              juce::ParameterID{ pid(d.suffix), 1 }, d.name, kSourceNames, static_cast<int>(d.defaultSource)));
        }

        return params;
    }

    const dsp::schema::AlgorithmSchema& schema() const override
    {
        return dsp::schema::modFactoryOneSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ModFactoryOneAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace modfactoryone_detail;
        auto pid = [](const char* suffix) { return prefixedId("modFactoryOne", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setBpm(v("bpm"));
        engine_.setKnob1(v("knob1"));
        engine_.setKnob2(v("knob2"));
        engine_.setMix(v("mix"));

        for (int i = 0; i < 2; ++i)
        {
            auto n = juce::String(i + 1);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };
            auto cv = [&](const char* base) { return paramValue(apvts, chId(base)); };

            engine_.setFilterCutoff(i, cv("filterCutoff"));
            engine_.setFilterQ(i, cv("filterQ"));
            engine_.setFilterType(i, static_cast<ModFactoryOne::FilterType>(static_cast<int>(cv("filterType"))));
            engine_.setFilterModAmount(i, cv("filterMod"));

            engine_.setDelayMs(i, cv("delayMs"));
            engine_.setDelayBpmBeats(i, cv("delayBpm"));
            engine_.setDelayFeedback(i, cv("delayFeedback"));
            engine_.setDelayLoop(i, cv("delayLoop") >= 0.5f);
            engine_.setDelayModMs(i, cv("delayMod"));

            engine_.setLfoFrequency(i, cv("lfoFreq"));
            engine_.setLfoBpmBeats(i, cv("lfoBpm"));
            engine_.setLfoWaveform(i, static_cast<dsp::MultiWaveLFO::Waveform>(static_cast<int>(cv("lfoWaveform"))));
            engine_.setLfoThresholdDb(i, cv("lfoThreshold"));
            engine_.setLfoModAmount(i, cv("lfoMod"));

            engine_.setEnvAttackMs(i, cv("envAttack"));
            engine_.setEnvDecayMs(i, cv("envDecay"));
            engine_.setEnvThresholdDb(i, cv("envThreshold"));
            engine_.setEnvRatio(i, cv("envRatio"));

            engine_.setAmpModAmount(i, cv("ampModAmount"));
            engine_.setAmpModOffset(i, cv("ampModOffset"));

            engine_.setModScaleAmount(i, cv("modScaleAmount"));
        }

        for (int i = 0; i < 4; ++i)
        {
            auto n = juce::String(i + 1);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };
            engine_.setMixAAmount(i, paramValue(apvts, chId("mixAAmount")));
            engine_.setMixBAmount(i, paramValue(apvts, chId("mixBAmount")));
        }

        for (const auto& d : kDestinations)
        {
            engine_.setPatch(d.destination, static_cast<Source>(static_cast<int>(v(d.suffix))));
        }

        engine_.process(left, right);
    }

  private:
    dsp::graphs::ModFactoryOneAlgorithm engine_;
};
}
