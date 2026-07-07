#include "StereoShiftPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/StereoShiftSchema.h"

#include <span>

namespace
{
constexpr auto kGrainId = "grain";
constexpr auto kDelayId = "delay";
constexpr auto kCentsId = "cents";
constexpr auto kFeedbackId = "feedback";
constexpr auto kMixId = "mix";
constexpr auto kInLevelLeftId = "inLevelLeft";
constexpr auto kInLevelRightId = "inLevelRight";

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

EventideStereoShiftAudioProcessor::EventideStereoShiftAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideStereoShiftAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideStereoShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kGrainId, "Grain", 0.01f, 0.3f, 0.07f, "s"));
    params.push_back(floatParam(kDelayId, "Delay", 0.0f, 0.5f, 0.05f, "s"));
    params.push_back(floatParam(kCentsId, "Shift", -2400.0f, 1200.0f, 700.0f, "cents"));
    params.push_back(floatParam(kFeedbackId, "Feedback", 0.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kMixId, "Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kInLevelLeftId, "In Level L", -1.0f, 1.0f, 1.0f));
    params.push_back(floatParam(kInLevelRightId, "In Level R", -1.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void EventideStereoShiftAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::StereoShiftAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideStereoShiftAudioProcessor::releaseResources() {}

bool EventideStereoShiftAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideStereoShiftAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setGrainSeconds(paramValue(kGrainId));
    engine_.setDelaySeconds(paramValue(kDelayId));
    engine_.setCents(paramValue(kCentsId));
    engine_.setFeedback(paramValue(kFeedbackId));
    engine_.setMix(paramValue(kMixId));
    engine_.setInLevel(paramValue(kInLevelLeftId), paramValue(kInLevelRightId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideStereoShiftAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::stereoShiftSchema());
}

void EventideStereoShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideStereoShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideStereoShiftAudioProcessor();
}
