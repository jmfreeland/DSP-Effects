#include "DualDigiplexPluginProcessor.h"

#include "LoomPluginEditor.h"
#include "dsp/schema/DualDigiplexSchema.h"

#include <span>

namespace
{
constexpr auto kLeftDelayId = "leftDelay";
constexpr auto kRightDelayId = "rightDelay";
constexpr auto kLeftFeedbackId = "leftFeedback";
constexpr auto kRightFeedbackId = "rightFeedback";
constexpr auto kGlideResponseId = "glideResponse";
constexpr auto kGlideEnabledId = "glideEnabled";
constexpr auto kRepeatId = "repeat";
constexpr auto kLeftMixId = "leftMix";
constexpr auto kRightMixId = "rightMix";
constexpr auto kStereoInputId = "stereoInput";

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

EventideDualDigiplexAudioProcessor::EventideDualDigiplexAudioProcessor()
  : AudioProcessor(BusesProperties()
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

float EventideDualDigiplexAudioProcessor::paramValue(const char* id) const
{
    return apvts.getRawParameterValue(id)->load();
}

juce::AudioProcessorValueTreeState::ParameterLayout
EventideDualDigiplexAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(floatParam(kLeftDelayId, "Left Delay", 0.0f, 0.7f, 0.2f, "s"));
    params.push_back(floatParam(kRightDelayId, "Right Delay", 0.0f, 0.7f, 0.3f, "s"));
    params.push_back(floatParam(kLeftFeedbackId, "Left Feedback", -1.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kRightFeedbackId, "Right Feedback", -1.0f, 0.99f, 0.0f));
    params.push_back(floatParam(kGlideResponseId, "Glide Speed", 0.0f, 100.0f, 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kGlideEnabledId, 1 },
                                                                  "Glide Enabled", true));
    params.push_back(
      std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kRepeatId, 1 }, "Repeat", false));
    params.push_back(floatParam(kLeftMixId, "Left Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(floatParam(kRightMixId, "Right Mix", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{ kStereoInputId, 1 },
                                                                  "Stereo Input", true));

    return { params.begin(), params.end() };
}

void EventideDualDigiplexAudioProcessor::prepareToPlay(double sampleRate, int /* samplesPerBlock */)
{
    workingBuffer_.assign(dsp::graphs::DualDigiplexAlgorithm::requiredWorkingBufferSize(), 0.0f);
    engine_.prepare(static_cast<float>(sampleRate), workingBuffer_);
}

void EventideDualDigiplexAudioProcessor::releaseResources() {}

bool EventideDualDigiplexAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void EventideDualDigiplexAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    engine_.setGlide(paramValue(kGlideResponseId), paramValue(kGlideEnabledId) >= 0.5f);
    engine_.setLeftDelaySeconds(paramValue(kLeftDelayId));
    engine_.setRightDelaySeconds(paramValue(kRightDelayId));
    engine_.setLeftFeedback(paramValue(kLeftFeedbackId));
    engine_.setRightFeedback(paramValue(kRightFeedbackId));
    engine_.setRepeat(paramValue(kRepeatId) >= 0.5f);
    engine_.setLeftMix(paramValue(kLeftMixId));
    engine_.setRightMix(paramValue(kRightMixId));
    engine_.setStereoInput(paramValue(kStereoInputId) >= 0.5f);

    auto numSamples = buffer.getNumSamples();
    std::span<float> left(buffer.getWritePointer(0), static_cast<std::size_t>(numSamples));
    std::span<float> right(buffer.getWritePointer(1), static_cast<std::size_t>(numSamples));
    engine_.process(left, right);
}

juce::AudioProcessorEditor* EventideDualDigiplexAudioProcessor::createEditor()
{
    return new LoomPluginEditor(*this, dsp::schema::dualDigiplexSchema());
}

void EventideDualDigiplexAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void EventideDualDigiplexAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new EventideDualDigiplexAudioProcessor();
}
