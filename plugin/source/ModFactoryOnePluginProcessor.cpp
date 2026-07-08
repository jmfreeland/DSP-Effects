#include "ModFactoryOnePluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/ModFactoryOneSchema.h"

#include <array>
#include <span>

namespace
{
using dsp::algorithms::ModFactoryOne;
using Destination = ModFactoryOne::Destination;
using Source = ModFactoryOne::Source;

// Order matches dsp::algorithms::ModFactoryOne::Source exactly, so the
// AudioParameterChoice index can be cast straight to the enum.
const juce::StringArray kSourceNames = {
    "Zero",      "Left Input", "Right Input", "Mixer 1",  "Mixer 2",     "Mixer 3",    "Mixer 4",
    "Amp Mod 1", "Amp Mod 2",  "Delay 1",     "Delay 2",  "Filter 1",    "Filter 2",   "Ducker 1",
    "Ducker 2",  "Envelope 1", "Envelope 2",  "LFO 1",    "LFO 2",       "Knob 1",     "Knob 2",
    "Noise Gen", "Fullscale",  "-Fullscale",  "ModScale 1", "ModScale 2"
};

const juce::StringArray kLfoWaveformNames = { "Sine",
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
const juce::StringArray kFilterTypeNames = { "Lowpass", "Bandpass", "Highpass" };

struct DestinationInfo
{
    Destination destination;
    const char* id;
    const char* name;
    Source defaultSource;
};

// clang-format off
const std::array<DestinationInfo, static_cast<std::size_t>(Destination::kCount)> kDestinations = { {
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

std::unique_ptr<juce::AudioParameterFloat> floatParam(const char* id, const juce::String& name,
                                                        float min, float max, float defaultValue,
                                                        const char* label = nullptr)
{
    juce::AudioParameterFloatAttributes attributes;
    if (label != nullptr)
    {
        attributes = attributes.withLabel(label);
    }
    auto interval = (max - min) / 10000.0f;
    return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{ id, 1 }, name, juce::NormalisableRange<float>(min, max, interval), defaultValue,
      attributes);
}
}

EventideModFactoryOneAudioProcessor::EventideModFactoryOneAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideModFactoryOneAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout EventideModFactoryOneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam("bpm", "BPM", 30.0f, 200.0f, 120.0f));
    params.push_back(floatParam("knob1", "Knob 1", 0.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam("knob2", "Knob 2", 0.0f, 100.0f, 0.0f, "%"));
    params.push_back(floatParam("mix", "Mix", 0.0f, 100.0f, 100.0f, "%"));

    for (int i = 1; i <= 2; ++i)
    {
        auto n = juce::String(i);
        params.push_back(floatParam(("filterCutoff" + n).toRawUTF8(), "Filter " + n + " Cutoff", 0.0f, 7000.0f,
                                     1000.0f, "Hz"));
        params.push_back(floatParam(("filterQ" + n).toRawUTF8(), "Filter " + n + " Q", 1.0f, 1000.0f, 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ ("filterType" + n).toRawUTF8(), 1 }, "Filter " + n + " Type", kFilterTypeNames, 0));
        params.push_back(
          floatParam(("filterMod" + n).toRawUTF8(), "Filter " + n + " Mod Amount", 0.0f, 7000.0f, 0.0f, "Hz"));

        params.push_back(floatParam(("delayMs" + n).toRawUTF8(), "Delay " + n, 0.0f, 700.0f, 300.0f, "ms"));
        params.push_back(
          floatParam(("delayBpm" + n).toRawUTF8(), "Delay " + n + " BPM", 0.0f, 96.0f, 0.0f, "/24"));
        params.push_back(
          floatParam(("delayFeedback" + n).toRawUTF8(), "Delay " + n + " Feedback", -100.0f, 100.0f, 0.0f, "%"));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
          juce::ParameterID{ ("delayLoop" + n).toRawUTF8(), 1 }, "Delay " + n + " Loop", false));
        params.push_back(
          floatParam(("delayMod" + n).toRawUTF8(), "Delay " + n + " Mod Amount", -500.0f, 500.0f, 0.0f, "ms"));

        params.push_back(floatParam(("lfoFreq" + n).toRawUTF8(), "LFO " + n + " Freq", 0.0f, 300.0f, 1.0f, "Hz"));
        params.push_back(
          floatParam(("lfoBpm" + n).toRawUTF8(), "LFO " + n + " BPM", 0.0f, 96.0f, 0.0f, "/24"));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ ("lfoWaveform" + n).toRawUTF8(), 1 }, "LFO " + n + " Waveform", kLfoWaveformNames, 0));
        params.push_back(
          floatParam(("lfoThreshold" + n).toRawUTF8(), "LFO " + n + " Threshold", -40.0f, 0.0f, -20.0f, "dB"));
        params.push_back(
          floatParam(("lfoMod" + n).toRawUTF8(), "LFO " + n + " Mod Amount", 0.0f, 300.0f, 0.0f, "Hz"));

        params.push_back(floatParam(("envAttack" + n).toRawUTF8(), "Env " + n + " Attack", 0.0f, 1000.0f, 5.0f, "ms"));
        params.push_back(floatParam(("envDecay" + n).toRawUTF8(), "Env " + n + " Decay", 0.0f, 1000.0f, 100.0f, "ms"));
        params.push_back(
          floatParam(("envThreshold" + n).toRawUTF8(), "Env " + n + " Threshold", -40.0f, 0.0f, -20.0f, "dB"));
        params.push_back(floatParam(("envRatio" + n).toRawUTF8(), "Env " + n + " Ratio", 1.0f, 100.0f, 4.0f, ":1"));

        params.push_back(
          floatParam(("ampModAmount" + n).toRawUTF8(), "AmpMod " + n + " Amount", -200.0f, 200.0f, 100.0f, "%"));
        params.push_back(
          floatParam(("ampModOffset" + n).toRawUTF8(), "AmpMod " + n + " Offset", -200.0f, 200.0f, 0.0f, "%"));

        params.push_back(
          floatParam(("modScaleAmount" + n).toRawUTF8(), "ModScale " + n + " Amount", -100.0f, 100.0f, 100.0f, "%"));
    }

    for (int i = 1; i <= 4; ++i)
    {
        auto n = juce::String(i);
        params.push_back(
          floatParam(("mixAAmount" + n).toRawUTF8(), "Mixer " + n + " A Amount", -100.0f, 100.0f,
                      i == 1 ? 100.0f : 0.0f, "%"));
        params.push_back(
          floatParam(("mixBAmount" + n).toRawUTF8(), "Mixer " + n + " B Amount", -100.0f, 100.0f,
                      i == 1 ? 100.0f : 0.0f, "%"));
    }

    for (const auto& d : kDestinations)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
          juce::ParameterID{ d.id, 1 }, d.name, kSourceNames, static_cast<int>(d.defaultSource)));
    }

    return { params.begin(), params.end() };
}

void EventideModFactoryOneAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::ModFactoryOneAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideModFactoryOneAudioProcessor::releaseResources() {}

bool EventideModFactoryOneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideModFactoryOneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setBpm(paramValue("bpm"));
    engine_.setKnob1(paramValue("knob1"));
    engine_.setKnob2(paramValue("knob2"));
    engine_.setMix(paramValue("mix"));

    for (int i = 0; i < 2; ++i)
    {
        auto n = juce::String(i + 1);
        engine_.setFilterCutoff(i, paramValue(("filterCutoff" + n).toRawUTF8()));
        engine_.setFilterQ(i, paramValue(("filterQ" + n).toRawUTF8()));
        engine_.setFilterType(i, static_cast<ModFactoryOne::FilterType>(
                                    static_cast<int>(paramValue(("filterType" + n).toRawUTF8()))));
        engine_.setFilterModAmount(i, paramValue(("filterMod" + n).toRawUTF8()));

        engine_.setDelayMs(i, paramValue(("delayMs" + n).toRawUTF8()));
        engine_.setDelayBpmBeats(i, paramValue(("delayBpm" + n).toRawUTF8()));
        engine_.setDelayFeedback(i, paramValue(("delayFeedback" + n).toRawUTF8()));
        engine_.setDelayLoop(i, paramValue(("delayLoop" + n).toRawUTF8()) >= 0.5f);
        engine_.setDelayModMs(i, paramValue(("delayMod" + n).toRawUTF8()));

        engine_.setLfoFrequency(i, paramValue(("lfoFreq" + n).toRawUTF8()));
        engine_.setLfoBpmBeats(i, paramValue(("lfoBpm" + n).toRawUTF8()));
        engine_.setLfoWaveform(i, static_cast<dsp::MultiWaveLFO::Waveform>(
                                     static_cast<int>(paramValue(("lfoWaveform" + n).toRawUTF8()))));
        engine_.setLfoThresholdDb(i, paramValue(("lfoThreshold" + n).toRawUTF8()));
        engine_.setLfoModAmount(i, paramValue(("lfoMod" + n).toRawUTF8()));

        engine_.setEnvAttackMs(i, paramValue(("envAttack" + n).toRawUTF8()));
        engine_.setEnvDecayMs(i, paramValue(("envDecay" + n).toRawUTF8()));
        engine_.setEnvThresholdDb(i, paramValue(("envThreshold" + n).toRawUTF8()));
        engine_.setEnvRatio(i, paramValue(("envRatio" + n).toRawUTF8()));

        engine_.setAmpModAmount(i, paramValue(("ampModAmount" + n).toRawUTF8()));
        engine_.setAmpModOffset(i, paramValue(("ampModOffset" + n).toRawUTF8()));

        engine_.setModScaleAmount(i, paramValue(("modScaleAmount" + n).toRawUTF8()));
    }

    for (int i = 0; i < 4; ++i)
    {
        auto n = juce::String(i + 1);
        engine_.setMixAAmount(i, paramValue(("mixAAmount" + n).toRawUTF8()));
        engine_.setMixBAmount(i, paramValue(("mixBAmount" + n).toRawUTF8()));
    }

    for (const auto& d : kDestinations)
    {
        engine_.setPatch(d.destination, static_cast<Source>(static_cast<int>(paramValue(d.id))));
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideModFactoryOneAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::modFactoryOneSchema());
}

void EventideModFactoryOneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideModFactoryOneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

// This creates new instances of the plugin.
// NOLINTNEXTLINE(readability-identifier-naming) - JUCE-mandated symbol name.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EventideModFactoryOneAudioProcessor();
}
