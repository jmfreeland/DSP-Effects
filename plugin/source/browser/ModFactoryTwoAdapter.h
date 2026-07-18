#pragma once

#include "AdapterHelpers.h"
#include "EngineAdapter.h"
#include "dsp/graphs/ModFactoryTwoAlgorithm.h"
#include "dsp/schema/ModFactoryTwoSchema.h"

#include <array>
#include <span>

// Adapts dsp::graphs::ModFactoryTwoAlgorithm to EngineAdapter - see
// ConcertHallAdapter.h's doc comment for the pattern; this is
// ModFactoryTwoPluginProcessor.cpp's own layout/processBlock,
// namespaced under "modFactoryTwo" (Algorithm 123, last of the H3000
// roadmap). A 28x22 patch matrix, one shared LFO/Envelope instead of
// mod factory|one's per-channel pair, and detuners built from the same
// Delay+PitchShiftVoice shape used elsewhere in this archive.
namespace loom::browser
{
namespace modfactorytwo_detail
{
using dsp::algorithms::ModFactoryTwo;
using Destination = ModFactoryTwo::Destination;
using Source = ModFactoryTwo::Source;

// Order matches dsp::algorithms::ModFactoryTwo::Source exactly.
inline const juce::StringArray kSourceNames = { "Zero",      "Left Input", "Right Input", "Mixer 1",    "Mixer 2",
                                                 "Mixer 3",   "Mixer 4",    "Amp Mod 1",   "Amp Mod 2",  "Delay 1",
                                                 "Delay 2",   "Detune 1",   "Detune 2",    "Ducker",     "Envelope",
                                                 "LFO",       "Mod Knob",   "Noise Gen",   "Fullscale",  "-Fullscale",
                                                 "ModScale 1", "ModScale 2" };

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

struct DestinationInfo
{
    Destination destination;
    const char* suffix;
    const char* name;
    Source defaultSource;
};

// clang-format off
inline const std::array<DestinationInfo, static_cast<std::size_t>(Destination::kCount)> kDestinations = { {
    { Destination::kLeftOut,    "dstLeftOut",    "Left Out",     Source::kMixer1 },
    { Destination::kRightOut,   "dstRightOut",   "Right Out",    Source::kMixer2 },
    { Destination::kMix1aIn,    "dstMix1aIn",    "Mix1a In",     Source::kLeftInput },
    { Destination::kMix1bIn,    "dstMix1bIn",    "Mix1b In",     Source::kDetune1 },
    { Destination::kMix2aIn,    "dstMix2aIn",    "Mix2a In",     Source::kLeftInput },
    { Destination::kMix2bIn,    "dstMix2bIn",    "Mix2b In",     Source::kDetune2 },
    { Destination::kMix3aIn,    "dstMix3aIn",    "Mix3a In",     Source::kZero },
    { Destination::kMix3bIn,    "dstMix3bIn",    "Mix3b In",     Source::kZero },
    { Destination::kMix4aIn,    "dstMix4aIn",    "Mix4a In",     Source::kZero },
    { Destination::kMix4bIn,    "dstMix4bIn",    "Mix4b In",     Source::kZero },
    { Destination::kAm1In,      "dstAm1In",      "AM1 In",       Source::kZero },
    { Destination::kAm1Mod,     "dstAm1Mod",     "AM1 Mod",      Source::kZero },
    { Destination::kAm2In,      "dstAm2In",      "AM2 In",       Source::kZero },
    { Destination::kAm2Mod,     "dstAm2Mod",     "AM2 Mod",      Source::kZero },
    { Destination::kDly1In,     "dstDly1In",     "Dly1 In",      Source::kZero },
    { Destination::kDly1Mod,    "dstDly1Mod",    "Dly1 Mod",     Source::kZero },
    { Destination::kDly1Ctmd,   "dstDly1Ctmd",   "Dly1 Ctmd",    Source::kZero },
    { Destination::kDly2In,     "dstDly2In",     "Dly2 In",      Source::kZero },
    { Destination::kDly2Mod,    "dstDly2Mod",    "Dly2 Mod",     Source::kZero },
    { Destination::kDly2Ctmd,   "dstDly2Ctmd",   "Dly2 Ctmd",    Source::kZero },
    { Destination::kDtune1In,   "dstDtune1In",   "Dtune1 In",    Source::kLeftInput },
    { Destination::kDtune1Mod,  "dstDtune1Mod",  "Dtune1 Mod",   Source::kZero },
    { Destination::kDtune2In,   "dstDtune2In",   "Dtune2 In",    Source::kLeftInput },
    { Destination::kDtune2Mod,  "dstDtune2Mod",  "Dtune2 Mod",   Source::kZero },
    { Destination::kEnvIn,      "dstEnvIn",      "Env In",       Source::kZero },
    { Destination::kLfoIn,      "dstLfoIn",      "LFO In",       Source::kZero },
    { Destination::kMdScl1In,   "dstMdScl1In",   "ModScale1 In", Source::kZero },
    { Destination::kMdScl2In,   "dstMdScl2In",   "ModScale2 In", Source::kZero },
} };
// clang-format on
}

class ModFactoryTwoAdapter : public EngineAdapter
{
  public:
    const char* id() const override { return "modFactoryTwo"; }
    const char* displayName() const override { return "Eventide Mod Factory Two"; }

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> createParameters() const override
    {
        using namespace modfactorytwo_detail;
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
        auto pid = [](const char* suffix) { return prefixedId("modFactoryTwo", suffix); };

        params.push_back(floatParam(pid("bpm"), "BPM", 30.0f, 200.0f, 120.0f));
        params.push_back(floatParam(pid("modKnob"), "Mod Knob", 0.0f, 100.0f, 0.0f, "%"));
        params.push_back(floatParam(pid("mix"), "Mix", 0.0f, 100.0f, 100.0f, "%"));

        for (int i = 1; i <= 2; ++i)
        {
            auto n = juce::String(i);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };

            params.push_back(floatParam(chId("delayMs"), "Delay " + n, 0.0f, 650.0f, 300.0f, "ms"));
            params.push_back(floatParam(chId("delayBpm"), "Delay " + n + " BPM", 0.0f, 96.0f, 0.0f, "/24"));
            params.push_back(
              floatParam(chId("delayFeedback"), "Delay " + n + " Feedback", -100.0f, 100.0f, 0.0f, "%"));
            params.push_back(std::make_unique<juce::AudioParameterBool>(
              juce::ParameterID{ chId("delayLoop"), 1 }, "Delay " + n + " Loop", false));
            params.push_back(
              floatParam(chId("delayMod"), "Delay " + n + " Mod Amount", -500.0f, 500.0f, 0.0f, "ms"));
            params.push_back(
              floatParam(chId("delayHighcut"), "Delay " + n + " Highcut", 1.0f, 20000.0f, 20000.0f, "Hz"));
            params.push_back(
              floatParam(chId("delayHighcutMod"), "Delay " + n + " Highcut Mod", 0.0f, 20000.0f, 0.0f, "Hz"));

            params.push_back(
              floatParam(chId("detuneCents"), "Detune " + n, -100.0f, 100.0f, i == 1 ? -10.0f : 10.0f, "ct"));
            params.push_back(
              floatParam(chId("detuneDelay"), "Detune " + n + " Delay", 0.0f, 700.0f, 20.0f, "ms"));
            params.push_back(floatParam(chId("detuneBpm"), "Detune " + n + " BPM", 0.0f, 96.0f, 0.0f, "/24"));
            params.push_back(
              floatParam(chId("detuneMod"), "Detune " + n + " Mod Amount", -1200.0f, 1200.0f, 0.0f, "ct"));
            params.push_back(
              floatParam(chId("detuneSplice"), "Detune " + n + " Splice Length", 1.0f, 700.0f, 150.0f, "ms"));

            params.push_back(
              floatParam(chId("ampModAmount"), "AmpMod " + n + " Amount", -200.0f, 200.0f, 100.0f, "%"));
            params.push_back(floatParam(chId("ampModOffset"), "AmpMod " + n + " Offset", -200.0f, 200.0f, 0.0f, "%"));

            params.push_back(
              floatParam(chId("modScaleAmount"), "ModScale " + n + " Amount", -100.0f, 100.0f, 100.0f, "%"));
        }

        params.push_back(floatParam(pid("lfoFreq"), "LFO Freq", 0.0f, 300.0f, 1.0f, "Hz"));
        params.push_back(floatParam(pid("lfoBpm"), "LFO BPM", 0.0f, 96.0f, 0.0f, "/24"));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ pid("lfoWaveform"), 1 }, "LFO Waveform", kLfoWaveformNames, 0));
        params.push_back(floatParam(pid("lfoThreshold"), "LFO Threshold", -40.0f, 0.0f, -20.0f, "dB"));
        params.push_back(floatParam(pid("lfoMod"), "LFO Mod Amount", 0.0f, 300.0f, 0.0f, "Hz"));

        params.push_back(floatParam(pid("envAttack"), "Env Attack", 0.0f, 1000.0f, 5.0f, "ms"));
        params.push_back(floatParam(pid("envDecay"), "Env Decay", 0.0f, 1000.0f, 100.0f, "ms"));
        params.push_back(floatParam(pid("envThreshold"), "Env Threshold", -40.0f, 0.0f, -20.0f, "dB"));
        params.push_back(floatParam(pid("envRatio"), "Env Ratio", 1.0f, 100.0f, 4.0f, ":1"));

        for (int i = 1; i <= 4; ++i)
        {
            auto n = juce::String(i);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };
            params.push_back(floatParam(chId("mixAAmount"), "Mixer " + n + " A Amount", -100.0f, 100.0f,
                                         i <= 2 ? 100.0f : 0.0f, "%"));
            params.push_back(floatParam(chId("mixBAmount"), "Mixer " + n + " B Amount", -100.0f, 100.0f,
                                         i <= 2 ? 100.0f : 0.0f, "%"));
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
        return dsp::schema::modFactoryTwoSchema();
    }

    std::size_t requiredWorkingBufferSize() const override
    {
        return dsp::graphs::ModFactoryTwoAlgorithm::requiredWorkingBufferSize();
    }

    void prepare(float sampleRate, std::span<float> workingBuffer) override
    {
        engine_.prepare(sampleRate, workingBuffer);
    }

    void process(juce::AudioProcessorValueTreeState& apvts, std::span<float> left,
                std::span<float> right) override
    {
        using namespace modfactorytwo_detail;
        auto pid = [](const char* suffix) { return prefixedId("modFactoryTwo", suffix); };
        auto v = [&](const char* suffix) { return paramValue(apvts, pid(suffix)); };

        engine_.setBpm(v("bpm"));
        engine_.setModKnob(v("modKnob"));
        engine_.setMix(v("mix"));

        for (int i = 0; i < 2; ++i)
        {
            auto n = juce::String(i + 1);
            auto chId = [&](const char* base) { return pid((juce::String(base) + n).toRawUTF8()); };
            auto cv = [&](const char* base) { return paramValue(apvts, chId(base)); };

            engine_.setDelayMs(i, cv("delayMs"));
            engine_.setDelayBpmBeats(i, cv("delayBpm"));
            engine_.setDelayFeedback(i, cv("delayFeedback"));
            engine_.setDelayLoop(i, cv("delayLoop") >= 0.5f);
            engine_.setDelayModMs(i, cv("delayMod"));
            engine_.setDelayHighcutHz(i, cv("delayHighcut"));
            engine_.setDelayHighcutModHz(i, cv("delayHighcutMod"));

            engine_.setDetuneCents(i, cv("detuneCents"));
            engine_.setDetuneDelayMs(i, cv("detuneDelay"));
            engine_.setDetuneBpmBeats(i, cv("detuneBpm"));
            engine_.setDetuneModAmountCents(i, cv("detuneMod"));
            engine_.setDetuneSpliceLengthMs(i, cv("detuneSplice"));

            engine_.setAmpModAmount(i, cv("ampModAmount"));
            engine_.setAmpModOffset(i, cv("ampModOffset"));

            engine_.setModScaleAmount(i, cv("modScaleAmount"));
        }

        engine_.setLfoFrequency(v("lfoFreq"));
        engine_.setLfoBpmBeats(v("lfoBpm"));
        engine_.setLfoWaveform(static_cast<dsp::MultiWaveLFO::Waveform>(static_cast<int>(v("lfoWaveform"))));
        engine_.setLfoThresholdDb(v("lfoThreshold"));
        engine_.setLfoModAmount(v("lfoMod"));

        engine_.setEnvAttackMs(v("envAttack"));
        engine_.setEnvDecayMs(v("envDecay"));
        engine_.setEnvThresholdDb(v("envThreshold"));
        engine_.setEnvRatio(v("envRatio"));

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
    dsp::graphs::ModFactoryTwoAlgorithm engine_;
};
}
