#include "SweptReverbPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/SweptReverbSchema.h"

#include <array>
#include <span>

namespace
{
constexpr auto kMasterDelayId = "masterDelay";
constexpr auto kMasterRateId = "masterRate";
constexpr auto kMasterDepthId = "masterDepth";
constexpr auto kFeedbackId = "feedback";
constexpr auto kMixId = "mix";
constexpr auto kRepeatId = "repeat";

constexpr int kNumLines = dsp::algorithms::SweptReverb::kNumLines;

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

EventideSweptReverbAudioProcessor::EventideSweptReverbAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideSweptReverbAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideSweptReverbAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kMasterDelayId, "Master Delay", 0.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kMasterRateId, "Master Rate", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kMasterDepthId, "Master Depth", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kFeedbackId, "Feedback", -1.0f, 1.0f, 0.7f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kRepeatId, 1 }, "Repeat", false));

    static constexpr std::array<float, kNumLines> kDefaultDelaysMs = { 37, 53, 71, 97, 131, 179 };
    static constexpr std::array<float, kNumLines> kDefaultRates = { 25, 40, 55, 30, 45, 60 };
    for (int i = 0; i < kNumLines; ++i)
    {
        auto idx = static_cast<std::size_t>(i);
        params.push_back(
          floatParam(lineParamId("Delay", i), "Line " + juce::String(i + 1) + " Delay", 0.0f, 225.0f,
                     kDefaultDelaysMs[idx], "ms"));
        params.push_back(floatParam(lineParamId("Rate", i), "Line " + juce::String(i + 1) + " Rate", 0.0f,
                                     100.0f, kDefaultRates[idx]));
        params.push_back(
          floatParam(lineParamId("Depth", i), "Line " + juce::String(i + 1) + " Depth", 0.0f, 100.0f, 30.0f));
    }

    return { params.begin(), params.end() };
}

void EventideSweptReverbAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::SweptReverbAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideSweptReverbAudioProcessor::releaseResources() {}

bool EventideSweptReverbAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideSweptReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setMasterDelay(paramValue(kMasterDelayId));
    engine_.setMasterRate(paramValue(kMasterRateId));
    engine_.setMasterDepth(paramValue(kMasterDepthId));
    engine_.setFeedback(paramValue(kFeedbackId));
    engine_.setMix(paramValue(kMixId));
    engine_.setRepeat(paramValue(kRepeatId) >= 0.5f);

    for (int i = 0; i < kNumLines; ++i)
    {
        engine_.setLineDelayMs(i, paramValue(lineParamId("Delay", i).toRawUTF8()));
        engine_.setLineRate(i, paramValue(lineParamId("Rate", i).toRawUTF8()));
        engine_.setLineDepth(i, paramValue(lineParamId("Depth", i).toRawUTF8()));
    }

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideSweptReverbAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::sweptReverbSchema());
}

void EventideSweptReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideSweptReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideSweptReverbAudioProcessor();
}
