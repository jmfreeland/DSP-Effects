#include "LongDigiplexPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/LongDigiplexSchema.h"

#include <span>

namespace
{
constexpr auto kDelayId = "delay";
constexpr auto kFeedbackId = "feedback";
constexpr auto kGlideResponseId = "glideResponse";
constexpr auto kGlideEnabledId = "glideEnabled";
constexpr auto kRepeatId = "repeat";
constexpr auto kMixId = "mix";
constexpr auto kInLevelId = "inLevel";

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

EventideLongDigiplexAudioProcessor::EventideLongDigiplexAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideLongDigiplexAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideLongDigiplexAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kDelayId, "Delay", 0.0f, 1.4f, 0.3f, "s"));
    params.push_back(floatParam(kFeedbackId, "Feedback", -1.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kGlideResponseId, "Glide Speed", 0.0f, 100.0f, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kGlideEnabledId, 1 },
                                                                  "Glide Enabled", true));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kRepeatId, 1 }, "Repeat", false));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kInLevelId, "In Level", -1.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void EventideLongDigiplexAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::LongDigiplexAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideLongDigiplexAudioProcessor::releaseResources() {}

bool EventideLongDigiplexAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideLongDigiplexAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setGlide(paramValue(kGlideResponseId), paramValue(kGlideEnabledId) >= 0.5f);
    engine_.setDelaySeconds(paramValue(kDelayId));
    engine_.setFeedback(paramValue(kFeedbackId));
    engine_.setRepeat(paramValue(kRepeatId) >= 0.5f);
    engine_.setMix(paramValue(kMixId));
    engine_.setInLevel(paramValue(kInLevelId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideLongDigiplexAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::longDigiplexSchema());
}

void EventideLongDigiplexAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideLongDigiplexAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideLongDigiplexAudioProcessor();
}
