#include "ReverbFactoryPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/ReverbFactorySchema.h"

#include <array>
#include <span>

namespace
{
constexpr auto kPredelayId = "predelay";
constexpr auto kOnDecayId = "onDecay";
constexpr auto kOffDecayId = "offDecay";
constexpr auto kGateTimeId = "gateTime";
constexpr auto kGateSpeedId = "gateSpeed";
constexpr auto kGateThresholdId = "gateThreshold";
constexpr auto kGateEnabledId = "gateEnabled";
constexpr auto kEqCrossoverId = "eqCrossover";
constexpr auto kOnEqGainId = "onEqGain";
constexpr auto kOffEqGainId = "offEqGain";
constexpr auto kMixId = "mix";
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";

constexpr int kNumLines = dsp::algorithms::ReverbFactory::kNumLines;

juce::String lineParamId(const char* suffix, int line)
{
    return "line" + juce::String(line) + suffix;
}

std::unique_ptr<juce::AudioParameterFloat> floatParam(const juce::String& id, const juce::String& name,
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

EventideReverbFactoryAudioProcessor::EventideReverbFactoryAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideReverbFactoryAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideReverbFactoryAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kPredelayId, "Predelay", 0.0f, 0.5f, 0.02f, "s"));
    params.push_back(floatParam(kOnDecayId, "On Decay", 0.1f, 10.0f, 2.5f, "s"));
    params.push_back(floatParam(kOffDecayId, "Off Decay", 0.1f, 10.0f, 1.0f, "s"));
    params.push_back(floatParam(kGateTimeId, "Gate Time", 0.0f, 25.0f, 1.0f, "s"));
    params.push_back(floatParam(kGateSpeedId, "Gate Speed", 0.0f, 100.0f, 50.0f));
    params.push_back(floatParam(kGateThresholdId, "Gate Threshold", 0.0f, 1.0f, 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kGateEnabledId, 1 },
                                                                  "Gate Enabled", true));
    params.push_back(floatParam(kEqCrossoverId, "EQ Crossover", 200.0f, 8000.0f, 2000.0f, "Hz"));
    params.push_back(floatParam(kOnEqGainId, "On EQ Gain", -24.0f, 6.0f, 0.0f, "dB"));
    params.push_back(floatParam(kOffEqGainId, "Off EQ Gain", -24.0f, 6.0f, -6.0f, "dB"));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));

    static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 29, 43, 61, 79, 97, 113 };
    for (int i = 0; i < kNumLines; ++i)
    {
        params.push_back(floatParam(lineParamId("Delay", i), "Line " + juce::String(i + 1) + " Delay",
                                     1.0f, 113.0f, kDefaultDelaysMs[static_cast<std::size_t>(i)], "ms"));
    }

    return { params.begin(), params.end() };
}

void EventideReverbFactoryAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::ReverbFactoryAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideReverbFactoryAudioProcessor::releaseResources() {}

bool EventideReverbFactoryAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideReverbFactoryAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setPredelaySeconds(paramValue(kPredelayId));
    engine_.setOnDecaySeconds(paramValue(kOnDecayId));
    engine_.setOffDecaySeconds(paramValue(kOffDecayId));
    engine_.setGateTimeSeconds(paramValue(kGateTimeId));
    engine_.setGateSpeed(paramValue(kGateSpeedId));
    engine_.setGateThreshold(paramValue(kGateThresholdId));
    engine_.setGateEnabled(paramValue(kGateEnabledId) >= 0.5f);
    engine_.setEqCrossoverHz(paramValue(kEqCrossoverId));
    engine_.setOnEqGainDb(paramValue(kOnEqGainId));
    engine_.setOffEqGainDb(paramValue(kOffEqGainId));
    engine_.setMix(paramValue(kMixId));
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));

    for (int i = 0; i < kNumLines; ++i)
    {
        engine_.setLineDelayMs(i, paramValue(lineParamId("Delay", i).toRawUTF8()));
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideReverbFactoryAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::reverbFactorySchema());
}

void EventideReverbFactoryAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideReverbFactoryAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideReverbFactoryAudioProcessor();
}
