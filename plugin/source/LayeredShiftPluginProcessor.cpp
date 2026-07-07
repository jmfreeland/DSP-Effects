#include "LayeredShiftPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/LayeredShiftSchema.h"

#include <span>

namespace
{
constexpr auto kGrainId = "grain";
constexpr auto kLeftDelayId = "leftDelay";
constexpr auto kRightDelayId = "rightDelay";
constexpr auto kLeftCentsId = "leftCents";
constexpr auto kRightCentsId = "rightCents";
constexpr auto kLeftFeedbackId = "leftFeedback";
constexpr auto kRightFeedbackId = "rightFeedback";
constexpr auto kLeftMixId = "leftMix";
constexpr auto kRightMixId = "rightMix";
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

EventideLayeredShiftAudioProcessor::EventideLayeredShiftAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideLayeredShiftAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideLayeredShiftAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kGrainId, "Grain", 0.01f, 0.3f, 0.07f, "s"));
    params.push_back(floatParam(kLeftDelayId, "Left Delay", 0.0f, 1.0f, 0.05f, "s"));
    params.push_back(floatParam(kRightDelayId, "Right Delay", 0.0f, 1.0f, 0.05f, "s"));
    params.push_back(floatParam(kLeftCentsId, "Left Voice", -2400.0f, 1200.0f, 400.0f, "cents"));
    params.push_back(floatParam(kRightCentsId, "Right Voice", -2400.0f, 1200.0f, 700.0f, "cents"));
    params.push_back(floatParam(kLeftFeedbackId, "Left Feedback", 0.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kRightFeedbackId, "Right Feedback", 0.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kLeftMixId, "Left Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kRightMixId, "Right Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kInLevelId, "In Level", -1.0f, 1.0f, 1.0f));

    return { params.begin(), params.end() };
}

void EventideLayeredShiftAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::LayeredShiftAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideLayeredShiftAudioProcessor::releaseResources() {}

bool EventideLayeredShiftAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideLayeredShiftAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setGrainSeconds(paramValue(kGrainId));
    engine_.setLeftDelaySeconds(paramValue(kLeftDelayId));
    engine_.setRightDelaySeconds(paramValue(kRightDelayId));
    engine_.setLeftCents(paramValue(kLeftCentsId));
    engine_.setRightCents(paramValue(kRightCentsId));
    engine_.setLeftFeedback(paramValue(kLeftFeedbackId));
    engine_.setRightFeedback(paramValue(kRightFeedbackId));
    engine_.setLeftMix(paramValue(kLeftMixId));
    engine_.setRightMix(paramValue(kRightMixId));
    engine_.setInLevel(paramValue(kInLevelId));

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideLayeredShiftAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::layeredShiftSchema());
}

void EventideLayeredShiftAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideLayeredShiftAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideLayeredShiftAudioProcessor();
}
